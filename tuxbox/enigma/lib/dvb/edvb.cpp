#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef PROFILE
#include <sys/time.h>
#endif
#include "edvb.h"
#include "esection.h"
#include "si.h"
#include "frontend.h"
#include "dvb.h"
#include "decoder.h"
#include <qfile.h>
#include <dbox/info.h>
#include "eavswitch.h"
#include "init.h"
#include "config.h"
#include <algorithm>
#include "streamwd.h"

eDVB *eDVB::instance;

std::string eDVB::getVersion()
{
	return "eDVB core 1.0, compiled " __DATE__;
}

void eDVB::removeDVBBouquets()
{
	for (BouquetIterator i = bouquets.begin(); i != bouquets.end();)
	{
		if ( (*i)->bouquet_id >= 0)
		{
			printf("removing bouquet '%s'\n", (*i)->bouquet_name.c_str());
			delete *i;
			bouquets.erase(i);
			i = bouquets.begin();
		}
		else
		{
			printf("leaving bouquet '%s'\n", (const char*)(*i)->bouquet_name.c_str());
			i++;
		}
	}
}

void eDVB::addDVBBouquet(BAT *bat)
{
	printf("wir haben da eine bat, id %x\n", bat->bouquet_id);
	std::string bouquet_name="Weiteres Bouquet";
	for (QListIterator<Descriptor> i(bat->bouquet_descriptors); i.current(); ++i)
	{
		Descriptor *d=i.current();
		if (d->Tag()==DESCR_BOUQUET_NAME)
			bouquet_name=((BouquetNameDescriptor*)d)->name;
	}
	eBouquet *bouquet=createBouquet(bat->bouquet_id, bouquet_name);
	
	for (QListIterator<BATEntry> be(bat->entries); be.current(); ++be)
		for (QListIterator<Descriptor> i(be.current()->transport_descriptors); i.current(); ++i)
			if (i.current()->Tag()==DESCR_SERVICE_LIST)
			{
				ServiceListDescriptor *s=(ServiceListDescriptor*)i.current();
				for (QListIterator<ServiceListDescriptorEntry> a(s->entries); a.current(); ++a)
					bouquet->add(be.current()->transport_stream_id, be.current()->original_network_id, a.current()->service_id);
			}
}

eBouquet *eDVB::getBouquet(int bouquet_id)
{
	for (BouquetIterator i = bouquets.begin(); i != bouquets.end(); i++)
		if ((*i)->bouquet_id==bouquet_id)
			return *i;
	return 0;
}

static std::string beautifyBouquetName(std::string bouquet_name)
{
	if (bouquet_name.find("ARD")!=-1)
		bouquet_name="ARD";		
	if (bouquet_name=="ABsat")
		bouquet_name="AB sat";
	if (bouquet_name=="Astra-Net")
		bouquet_name="ASTRA";
/*	if (bouquet_name=="CANALSATELLITE")
		bouquet_name="CANAL SATELLITE"; */
	if (bouquet_name.find("SES")!=-1)
		bouquet_name="SES Multimedia";
	if (bouquet_name=="\x05ZDF.vision")	// ja ich weiss was \x05 bedeutet - SP�TER
		bouquet_name="ZDF.vision";
	return bouquet_name;
}

eBouquet *eDVB::getBouquet(std::string bouquet_name)
{
	for (BouquetIterator i = bouquets.begin(); i != bouquets.end(); i++)
		if (!stricmp((*i)->bouquet_name.c_str(), bouquet_name.c_str()))
			return (*i);
	return 0;
}

eBouquet* eDVB::createBouquet(int bouquet_id, std::string bouquet_name)
{
	eBouquet *n=getBouquet(bouquet_id);
	if (!n)
		bouquets.push_back(n=new eBouquet(bouquet_id, bouquet_name));
	return n;
}

eBouquet *eDVB::createBouquet(std::string bouquet_name)
{
	eBouquet *n=getBouquet(bouquet_name);
	if (!n)
	{
		int bouquet_id=getUnusedBouquetID(0);
		bouquets.push_back(n=new eBouquet(bouquet_id, bouquet_name));
	}
	return n;
}

int eDVB::getUnusedBouquetID(int range)
{
	if (range)
		range=-1;
	else
		range=1;
	for (int bouquet_id=0; ; bouquet_id+=range)
		if (!getBouquet(bouquet_id))
			return bouquet_id;
}

void eDVB::revalidateBouquets()
{
	printf("revalidating bouquets\n");
	if (transponderlist)
		for (BouquetIterator i = bouquets.begin(); i != bouquets.end(); i++)
			for (ServiceReferenceIterator service = (*i)->list.begin(); service != (*i)->list.end(); service++)
				(*service)->service=transponderlist->searchService((*service)->original_network_id, (*service)->service_id);
	emit bouquetListChanged();
	printf("ok\n");
}

int eDVB::checkCA(std::list<CA*> &list, const QList<Descriptor> &descriptors)
{
	int found=0;
	for (QListIterator<Descriptor> i(descriptors); i.current(); ++i)
	{
		if (i.current()->Tag()==9)	// CADescriptor
		{
			found++;
			CADescriptor *ca=(CADescriptor*)i.current();
			Decoder::addCADescriptor((__u8*)(ca->data));
			int avail=0;
			for (IntIterator i = availableCASystems.begin(); i != availableCASystems.end() && !avail; i++)
				if (*i == ca->CA_system_ID)
					avail++;
			if (avail)
			{
				for (CAIterator a = list.begin(); a != list.end(); a++)
					if ((*a)->casysid==ca->CA_system_ID)
					{
						avail=0;
						break;
					}
				if (avail)
				{
					CA *n=new CA;
					n->ecmpid=ca->CA_PID;
					n->casysid=ca->CA_system_ID;
					n->emmpid=-1;					
					list.push_back(n);
				}
			}
		}
	}
	return found;
}

void eDVB::scanEvent(int event)
{
	emit eventOccured(event);
	switch (event)
	{
	case eventScanBegin:
		Decoder::Flush();
		if (transponderlist)
			delete transponderlist;
		transponderlist=new eTransponderList();
		if (useBAT)
			removeDVBBouquets();
		emit serviceListChanged();
		if (!initialTransponders)
		{
			qFatal("no initial transponders");
			scanEvent(eventScanCompleted);
			break;
		}
		for (std::list<eTransponder*>::const_iterator i = initialTransponders->begin(); i != initialTransponders->end(); ++i)
		{
			eTransponder &t=transponderlist->createTransponder((*i)->transport_stream_id, (*i)->original_network_id);
			t.set(*(*i));
		}
		currentONID=-1;
		knownNetworks.clear();
		scanEvent(eventScanNext);
		break;
	case eventScanNext:
	{
		eTransponder *next=transponderlist->getFirstTransponder(eTransponder::stateListed);
		transponder=next;
		
		if (!next)
			scanEvent(eventScanCompleted);
		else
		{
			if (next->tune())
				scanEvent(eventScanError);
			else
				setState(stateScanTune);
		}
		break;
	}
	case eventScanTuneError:
		scanEvent(eventScanError);
		break;
	case eventScanTuneOK:
		setState(stateScanGetPAT);
		tPAT.get();
		break;
	case eventScanGotPAT:
	{
		if (state!=stateScanGetPAT)
			qFatal("unexpected gotPAT");

		if (!tPAT.ready())
			qFatal("tmb suckt -> no pat");
		PAT *pat=tPAT.getCurrent();
		int nitpid;
		PATEntry *pe=pat->searchService(0);
		if (!pe)
		{
			printf("no NIT-PMTentry, assuming 0x10\n");
			nitpid=0x10;
		}	else
			nitpid=pe->program_map_PID;
		pat->unlock();
		scanOK=0;
		tNIT.start(new NIT(nitpid));
		if (scanflags&SCAN_ONIT)
			tONIT.start(new NIT(nitpid, NIT::typeOther));
		else
			scanOK|=8;
		tSDT.start(new SDT());
		if (useBAT)
			tBAT.start(new BAT());
		else
			scanOK|=4;
		setState(stateScanWait);
		break;
	}
	case eventScanGotSDT:
	{
		SDT *sdt=tSDT.ready()?tSDT.getCurrent():0;
		if (sdt)
		{
			transponder->transport_stream_id=sdt->transport_stream_id;
			transponder->original_network_id=sdt->original_network_id;
			
			currentONID=sdt->original_network_id;
			int known=0;

			for (IntIterator It = knownNetworks.begin(); It != knownNetworks.end(); It++)
				if (*It == sdt->original_network_id)
					known=1;
			
			if (known)
			{
				if (scanflags&SCAN_SKIP)
				{
					tNIT.abort();
					tONIT.abort();
				}
			}
			transponderlist->handleSDT(sdt);
			sdt->unlock();
		}
		scanOK|=1;
		printf("scanOK %d\n", scanOK);
		if (scanOK==15)
			scanEvent(eventScanComplete);
		break;
	}
	case eventScanGotNIT:
	case eventScanGotONIT:
	{
		NIT *nit=(event==eventScanGotNIT)?(tNIT.ready()?tNIT.getCurrent():0):(tONIT.ready()?tONIT.getCurrent():0);
		if (nit)
		{
			if (event==eventScanGotNIT)
				if (currentONID!=-1)
					knownNetworks.push_back(currentONID);
#if 0
			for (QListIterator<Descriptor> i(nit->network_descriptor; i.current(); ++i)
			{
				Descriptor *d=i.current();
				if (d->Tag()==DESCR_LINKAGE)
				{
					LinkageDescriptor *l=(LinkageDescriptor*)d;
					if ((l->linkage_type==0x01) && 		// information service
							(original_network_id==transponder->original_network_id) &&
							(transport_stream_id==transponder->transport_stream_id))
					{
						tSDT.abort();
					}
				}
			}
#endif
			for (QListIterator<NITEntry> i(nit->entries); i.current(); ++i)
			{
				NITEntry *entry=i.current();
				eTransponder &transponder=transponderlist->createTransponder(entry->transport_stream_id, entry->original_network_id);
				for (QListIterator<Descriptor> d(entry->transport_descriptor); d.current(); ++d)
				{
					switch (d.current()->Tag())
					{
					case DESCR_SAT_DEL_SYS:
						transponder.setSatellite((SatelliteDeliverySystemDescriptor*)d.current());
						break;
					case DESCR_CABLE_DEL_SYS:
						transponder.setCable((CableDeliverySystemDescriptor*)d.current());
						break;
					}
				}
			}
			nit->unlock();
		}
		scanOK|=(event==eventScanGotNIT)?2:8;
		printf("scanOK %d\n", scanOK);
		if (scanOK==15)
			scanEvent(eventScanComplete);
		break;
	}
	case eventScanGotBAT:
	{
		BAT *bat=tBAT.ready()?tBAT.getCurrent():0;
		if (bat)
		{
			addDVBBouquet(bat);
			bat->unlock();
		}
		scanOK|=4;
		printf("scanOK %d\n", scanOK);
		if (scanOK==15)
			scanEvent(eventScanComplete);
		break;
	}
	case eventScanError:
		printf("with error\n");
	case eventScanComplete:
		printf("completed\n");
		if (transponder)
		{
			if (event==eventScanError)
				transponder->state=eTransponder::stateError;
			else
				transponder->state=eTransponder::stateOK;
		}
		scanEvent(eventScanNext);
		break;
	case eventScanCompleted:
		printf("scan has finally completed.\n");
		saveServices();
		sortInChannels();
		emit serviceListChanged();
		setState(stateIdle);
		break;
	}
}

void eDVB::serviceEvent(int event)
{
	emit eventOccured(event);
#ifdef PROFILE
	static timeval last_event;
	
	timeval now;
	gettimeofday(&now, 0);
	
	int diff=(now.tv_sec-last_event.tv_sec)*1000000+(now.tv_usec-last_event.tv_usec);
	last_event=now;
	
	char *what="unknown";

	switch (event)
	{
	case	eventServiceSwitch: what="ServiceSwitch"; break;
	case	eventServiceTuneOK: what="TuneOK"; break;
	case	eventServiceTuneFailed: what="TuneFailed"; break;
	case	eventServiceGotPAT: what="GotPAT"; break;
	case	eventServiceGotPMT: what="GotPMT"; break;
	case	eventServiceNewPIDs: what="NewPIDs"; break;
	case	eventServiceGotSDT: what="GotSDT"; break;
	case	eventServiceSwitched: what="Switched"; break;
	case	eventServiceFailed: what="Failed"; break;

	}
	
	printf("[PROFILE] [%s] +%dus\n", what, diff);
#endif
	switch (event)
	{
	case eventServiceSwitch:
	{
		if (!transponderlist)
		{
			service_state=ENOENT;
			serviceEvent(eventServiceFailed);
			return;
		}
		eTransponder *n=transponderlist->searchTS(original_network_id, transport_stream_id);
		if (!n)
		{
			setState(eventServiceTuneFailed);
			break;
		}
		if (n->state!=eTransponder::stateOK)
		{
			printf("couldn't tune\n");
			service_state=ENOENT;
			serviceEvent(eventServiceFailed);
			return;
		}
		if (n==transponder)
		{
//			setState(stateServiceTune);
			serviceEvent(eventServiceTuneOK);
		} else
		{
			emit leaveTransponder(transponder);
			transponder=n;
			if (n->tune())
				serviceEvent(eventServiceTuneFailed);
			else
				setState(stateServiceTune);
		}
		printf("<-- tuned\n");
		break;
	}
	case eventServiceTuneOK:
		emit enterTransponder(transponder);
		tSDT.start(new SDT());
		switch (service_type)
		{
		case 1:	// digital television service
		case 2:	// digital radio service
		case 3:	// teletext service
			tEIT.start(new EIT(EIT::typeNowNext, service_id, EIT::tsActual));
		case 5:	// NVOD time shifted service
			setState(stateServiceGetPAT);
			tPAT.get();
			break;
		case 4:	// NVOD reference service
			setState(stateServiceGetSDT);
			tEIT.start(new EIT(EIT::typeNowNext, service_id, EIT::tsActual));
			break;
		case 6:	// mosaic service
			setState(stateServiceGetPAT);
			tPAT.get();
			break;
		case -1: // data
			setState(stateServiceGetPAT);
			tPAT.get();
			break;
		default:
			service_state=ENOSYS;
			serviceEvent(eventServiceFailed);
			break;
		}
		break;
	case eventServiceTuneFailed:
		printf("[TUNE] tune failed\n");
		service_state=ENOENT;
		serviceEvent(eventServiceFailed);
		break;
	case eventServiceGotPAT:
	{
		printf("eventServiceGotPAT\n");
		PAT *pat=tPAT.getCurrent();
		PATEntry *pe=pat->searchService(service_id);
		if (!pe)
		{
			pmtpid=-1;
		}	else
			pmtpid=pe->program_map_PID;
		pat->unlock();
		if (pmtpid==-1)
		{
			printf("[PAT] no pat entry\n");
			service_state=ENOENT;
			serviceEvent(eventServiceFailed);
			return;
		}
		tPMT.start(new PMT(pmtpid, service_id));
		setState(stateServiceGetPMT);
		break;
	}	
	case eventServiceGotPMT:
		printf("eventServiceGotPMT\n");
		service_state=0;
		scanPMT();
		{
			PMT *pmt=tPMT.ready()?tPMT.getCurrent():0;
			if (pmt)
			{
				emit gotPMT(pmt);
				pmt->unlock();
			}
		}
		if (state==stateServiceGetPMT)
			serviceEvent(eventServiceSwitched);
		else
			printf("nee, doch nicht\n");
		break;
	case eventServiceGotSDT:
	{
		printf("eventServiceGotSDT\n");
		SDT *sdt=tSDT.ready()?tSDT.getCurrent():0;
		if (sdt)
		{
			setState(stateIdle);
			emit gotSDT(sdt);
			sdt->unlock();
			if (service_type==4)
				service_state=ENVOD;
			serviceEvent(eventServiceSwitched);
		} else
			serviceEvent(eventServiceFailed);
		break;
	}
	case eventServiceNewPIDs:
		Decoder::Set();
		break;
	case eventServiceSwitched:
		emit enterService(service);
	case eventServiceFailed:
		emit switchedService(service, -service_state);
		setState(stateIdle);
		break;
	}
}

void eDVB::scanPMT()
{
	PMT *pmt=tPMT.ready()?tPMT.getCurrent():0;
	if (!pmt)
	{
		printf("scanPMT with no available pmt\n");
		return;
	}
	Decoder::parms.pmtpid=pmtpid;
	Decoder::parms.pcrpid=pmt->PCR_PID;
	Decoder::parms.ecmpid=Decoder::parms.emmpid=Decoder::parms.casystemid=-1;
	Decoder::parms.vpid=Decoder::parms.apid=-1;
	
	int isca=0;
	
	calist.clear();
	Decoder::parms.descriptor_length=0;

	isca+=checkCA(calist, pmt->program_info);
	
	PMTEntry *audio=0, *video=0, *teletext=0;
	
	for (QListIterator<PMTEntry> i(pmt->streams); i.current(); ++i)
	{
		PMTEntry *pe=i.current();
		switch (pe->stream_type)
		{
		case 1:	// ISO/IEC 11172 Video
		case 2: // ITU-T Rec. H.262 | ISO/IEC 13818-2 Video or ISO/IEC 11172-2 constrained parameter video stream
			isca+=checkCA(calist, pe->ES_info);
			if (!video)
				video=pe;
			break;
		case 3:	// ISO/IEC 11172 Audio
		case 4: // ISO/IEC 13818-3 Audio
			isca+=checkCA(calist, pe->ES_info);
			if (!audio)
				audio=pe;
			break;
		case 6:
		{
			isca+=checkCA(calist, pe->ES_info);
			for (QListIterator<Descriptor> i(pe->ES_info); i.current(); ++i)
			{
				Descriptor *d=i.current();
				/* if ((d->Tag()==DESCR_AC3))
					audio=pe; */
				if (d->Tag()==DESCR_TELETEXT)
					teletext=pe;
			}
			break;
		}
		case 0xC1:
		{
			if (tMHWEIT)	// nur eine zur zeit
				delete tMHWEIT;
			tMHWEIT=0;
			for (QListIterator<Descriptor> i(pe->ES_info); i.current(); ++i)
				if (i.current()->Tag()==DESCR_MHW_DATA)
				{
					MHWDataDescriptor *mhwd=(MHWDataDescriptor*)i.current();
					if (!strncmp(mhwd->type, "PILOTE", 6))
					{
						printf("starting MHWEIT on pid %x, sid %x\n", pe->elementary_PID, service_id);
						tMHWEIT=new MHWEIT(pe->elementary_PID, service_id);
						connect(tMHWEIT, SIGNAL(ready(int)), SLOT(MHWEITready(int)));
						tMHWEIT->start();
						break;
					}
				}
			break;
		}
		}
	}

	setPID(video);
	setPID(audio);
	setPID(teletext);

	emit scrambled(isca);

	if (isca && calist.empty())
	{
		printf("NO CASYS\n");
		service_state=ENOCASYS;
	}

	if ((Decoder::parms.vpid==-1) && (Decoder::parms.apid==-1))
		service_state=ENOSTREAM;

	for (CAIterator i = calist.begin(); i != calist.end(); ++i)
	{
		printf("CA %04x ECMPID %04x\n", (*i)->casysid, (*i)->ecmpid);
	}

	pmt->unlock();
	setDecoder();
}

void eDVB::tunedIn(eTransponder *trans, int err)
{
	currentTransponder=trans;
	currentTransponderState=err;
	if (!err)
		emit enterTransponder(trans);
	tPAT.start(new PAT());
	if (tdt)
		delete tdt;
	if (tMHWEIT)
		delete tMHWEIT;
	tMHWEIT=0;
	tdt=new TDT();
	connect(tdt, SIGNAL(tableReady(int)), SLOT(TDTready(int)));
	tdt->start();
	switch (state)
	{
	case stateIdle:
		break;
	case stateScanTune:
		if (transponder==trans)
			scanEvent(err?eventScanTuneError:eventScanTuneOK);
		break;
	case stateServiceTune:
		if (transponder==trans)
			serviceEvent(err?eventServiceTuneFailed:eventServiceTuneOK);
		break;		
	default:
		printf("wrong state %d\n", state);
	}
}

void eDVB::PATready(int error)
{
	switch (state)
	{
	case stateScanGetPAT:
		printf("stateScanGetPAT\n");
		scanEvent(error?eventScanError:eventScanGotPAT);
		break;
	case stateServiceGetPAT:
		printf("stateServiceGetPAT\n");
		serviceEvent(error?eventServiceFailed:eventServiceGotPAT);
		break;
	default:
		printf("anderer: %d\n", state);
	}
}

void eDVB::SDTready(int error)
{
	printf("SDTready %s\n", strerror(-error));
	switch (state)
	{
	case stateScanWait:
		scanEvent(eventScanGotSDT);
		break;
	case stateServiceGetSDT:
		serviceEvent(eventServiceGotSDT);
		break;
	}
	if (transponderlist)
	{
		SDT *sdt=tSDT.ready()?tSDT.getCurrent():0;
		if (sdt)
		{
			transponderlist->handleSDT(sdt);
			sdt->unlock();
		}
	}
}

void eDVB::PMTready(int error)
{
	printf("PMTready %s\n", strerror(-error));
	switch (state)
	{
	case stateIdle:
	case stateServiceGetPMT:
		serviceEvent(eventServiceGotPMT);
		break;
	}
}

void eDVB::NITready(int error)
{
	printf("NITready %s\n", strerror(-error));
	switch (state)
	{
	case stateScanWait:
		scanEvent(eventScanGotNIT);
		break;
	}
}

void eDVB::ONITready(int error)
{
	printf("ONITready %s\n", strerror(-error));
	switch (state)
	{
	case stateScanWait:
		scanEvent(eventScanGotONIT);
		break;
	}
}

void eDVB::EITready(int error)
{
	printf("EITready %s\n", strerror(-error));
	if (!error)
	{
		EIT *eit=tEIT.getCurrent();
		emit gotEIT(eit, 0);
		eit->unlock();
	} else
		emit gotEIT(0, error);
}

void eDVB::TDTready(int error)
{
	printf("TDTready %d\n", error);
	if (!error)
	{
		printf("[TIME] time update to %s\n", ctime(&tdt->UTC_time));
		time_difference=tdt->UTC_time-time(0);
		emit timeUpdated();
	}
}

void eDVB::BATready(int error)
{
	printf("BATready %s\n", strerror(-error));
	switch (state)
	{
	case stateScanWait:
		scanEvent(eventScanGotBAT);
		break;	
	}
}

void eDVB::MHWEITready(int error)
{
	if (!error)
	{
		EIT *e=new EIT();
		e->ts=EIT::tsFaked;
		e->type=EIT::typeNowNext;
		e->version_number=0;
		e->current_next_indicator=0;
		e->transport_stream_id=transport_stream_id;
		e->original_network_id=original_network_id;
		
		for (int i=0; i<2; i++)
		{
			MHWEITEvent *me=&tMHWEIT->events[i];
			EITEvent *ev=new EITEvent;
			int thisday=time_difference+time(0);
			thisday-=thisday%(60*60*24);
			if (thisday < (time_difference+time(0)))
				thisday+=60*60*24;
			e->service_id=me->service_id;
			ev->event_id=0xFFFF;
			ev->start_time=thisday+(me->starttime>>8)*60*60+(me->starttime&0xFF)*60;
			ev->duration=(me->duration>>8)*60*60+(me->duration&0xFF)*60;
			ev->running_status=1;
			ev->free_CA_mode=0;
			ShortEventDescriptor *se=new ShortEventDescriptor();
			se->language_code[0]='?';
			se->language_code[1]='?';
			se->language_code[2]='?';
			se->event_name=me->event_name;
			se->text=me->short_description;
			ev->descriptor.append(se);
			e->events.append(ev);
		}
		e->ready=1;
		tEIT.inject(e);
	} else
	{
		delete tMHWEIT;
		tMHWEIT=0;
	}
}

int eDVB::startScan(const std::list<eTransponder*> &initial, int flags)
{
	if (state!=stateIdle)
	{
		qFatal("BUSY");
		return -EBUSY;
	}
	scanflags=flags;
	initialTransponders=&initial;
	scanEvent(eventScanBegin);
	initialTransponders=0;
	return 0;
}

int eDVB::switchService(eService *newservice)
{
	if (newservice==service)
		return 0;
	emit leaveService(service);
	service=newservice;
	return switchService(service->service_id, service->original_network_id, service->transport_stream_id, service->service_type);
}

int eDVB::switchService(int nservice_id, int noriginal_network_id, int ntransport_stream_id, int nservice_type)
{
	original_network_id=noriginal_network_id;
	transport_stream_id=ntransport_stream_id;
	service_id=nservice_id;
	service_type=nservice_type;
	
	serviceEvent(eventServiceSwitch);
	printf("<--- switch service event\n");
	return 1;
}

eDVB::eDVB()
{
	config.setName(CONFIGDIR "/enigma/registry");
	if (config.open())
	{
		if (config.createNew())
		{
			mkdir(CONFIGDIR "/enigma", 0777);
			if (config.createNew())
				qFatal("error while opening/creating registry - create " CONFIGDIR "/enigma");
		}
		if (config.open())
			qFatal("still can't open configfile");
	}

	time_difference=0;
	if (config.getKey("/elitedvb/DVB/useBAT", useBAT))
		useBAT=0;
	if (instance)
		qFatal("eDVB already initialized!");
	instance=this;
	std::string frontend=getInfo("fe");
	int fe;
	if (!frontend.length())
	{
		qWarning("WARNING: couldn't determine frontend-type, assuming satellite...\n");
		fe=eFrontend::feSatellite;
	} else
	{
		switch(atoi(frontend.c_str()))
		{
		case DBOX_FE_CABLE:
			fe=eFrontend::feCable;
			break;
		case DBOX_FE_SAT:
			fe=eFrontend::feSatellite;
			break;
		default:
			qWarning("COOL: dvb-t is out. less cool: eDVB doesn't support it yet...\n");
			fe=eFrontend::feCable;
			break;
		}
	}
	if (eFrontend::open(fe)<0)
		qFatal("couldn't open frontend");
	connect(eFrontend::fe(), SIGNAL(tunedIn(eTransponder*,int)), SLOT(tunedIn(eTransponder*,int)));

	transponderlist=0;
	currentTransponder=0;
	currentTransponderState=-1;
	loadServices();
	loadBouquets();
	Decoder::Initialize();
	
	connect(&tPAT, SIGNAL(tableReady(int)), SLOT(PATready(int)));
	connect(&tPMT, SIGNAL(tableReady(int)), SLOT(PMTready(int)));
	connect(&tSDT, SIGNAL(tableReady(int)), SLOT(SDTready(int)));
	connect(&tNIT, SIGNAL(tableReady(int)), SLOT(NITready(int)));
	connect(&tONIT, SIGNAL(tableReady(int)), SLOT(ONITready(int)));
	connect(&tEIT, SIGNAL(tableReady(int)), SLOT(EITready(int)));
	connect(&tBAT, SIGNAL(tableReady(int)), SLOT(BATready(int)));
	
	setState(stateIdle);

	availableCASystems.push_back(0x1702);	// BetaCrypt C (sat)
	availableCASystems.push_back(0x1722);	// BetaCrypt D (cable)
	availableCASystems.push_back(0x1762);	// BetaCrypt F (ORF)
	
	service=0;
	tdt=0;

	int type=0;
	std::string mid=getInfo("mID");
	if (mid.length())
		type=atoi(mid.c_str());
	
	switch (type)
	{
	case 1:
		new eAVSwitchNokia;
		break;
	case 2:
		new eAVSwitchPhilips;
		break;
	case 3:
		new eAVSwitchSagem;
		break;
	default:
		new eAVSwitchNokia;
		break;
	}
	
	printf("instance: %p\n",  eAVSwitch::getInstance());
	eAVSwitch::getInstance()->setInput(0);
	eAVSwitch::getInstance()->setActive(1);
	eStreamWatchdog::getInstance()->reloadSettings();

	int vol, m;
	if (config.getKey("/elitedvb/audio/volume", vol))
		vol=10;
	if (config.getKey("/elitedvb/audio/mute", m))
		m=0;
	changeVolume(1, vol);
	changeVolume(3, m);
	
	tMHWEIT=0;
	
	printf("eDVB::eDVB done.\n");
}

eDVB::~eDVB()
{
	delete eAVSwitch::getInstance();
	config.setKey("/elitedvb/audio/volume", volume);
	config.setKey("/elitedvb/audio/mute", mute);
	Decoder::Close();

	if (!calist.empty())
	{
		CAIterator It = calist.begin();
		while (It != calist.end())
			delete *It++;
	}

	if (!bouquets.empty())
	{
		BouquetIterator It = bouquets.begin();
		while (It != bouquets.end())
			delete *It++;
	}

	if (transponderlist)
		delete transponderlist;
	eFrontend::close();
	instance=0;
	config.close();
}

eTransponderList *eDVB::getTransponders()
{
	return transponderlist;
}

std::list<eBouquet*>* eDVB::getBouquets()
{
	return &bouquets;
}

void eDVB::setTransponders(eTransponderList *tlist)
{
	if (transponderlist)
		delete transponderlist;
	transponderlist=tlist;
	emit serviceListChanged();
}

std::string eDVB::getInfo(const char *info)
{
	FILE *f=fopen("/proc/bus/dbox", "rt");
	if (!f)
		return 0;
	std::string result;
	while (1)
	{
		char buffer[128];
		if (!fgets(buffer, 128, f))
			break;
		if (strlen(buffer))
			buffer[strlen(buffer)-1]=0;
		if ((!strncmp(buffer, info, strlen(info)) && (buffer[strlen(info)]=='=')))
		{
			int i = strlen(info)+1;
			result = std::string(buffer).substr(i, strlen(buffer)-i);
			break;
		}
	}	
	fclose(f);
	return result;
}

void eDVB::setPID(PMTEntry *entry)
{
	if (entry)
	{
		int isvideo=0, isaudio=0, isteletext=0, isAC3=0;
		switch (entry->stream_type)
		{
		case 1:	// ISO/IEC 11172 Video
		case 2: // ITU-T Rec. H.262 | ISO/IEC 13818-2 Video or ISO/IEC 11172-2 constrained parameter video stream
			isvideo=1;
			break;
		case 3:	// ISO/IEC 11172 Audio
		case 4: // ISO/IEC 13818-3 Audio
			isaudio=1;
			break;
		case 6:
		{
			for (QListIterator<Descriptor> i(entry->ES_info); i.current(); ++i)
			{
				Descriptor *d=i.current();
				if (d->Tag()==DESCR_AC3)
				{
					isaudio=1;
					isAC3=1;
				}
				if (d->Tag()==DESCR_TELETEXT)
					isteletext=1;
			}
		}
		}
		if (isaudio)
		{
			Decoder::parms.audio_type=isAC3?DECODE_AUDIO_AC3:DECODE_AUDIO_MPEG;
			Decoder::parms.apid=entry->elementary_PID;
		}
		if (isvideo)
			Decoder::parms.vpid=entry->elementary_PID;
		if (isteletext)
			Decoder::parms.tpid=entry->elementary_PID;
	}
}

void eDVB::setDecoder()
{
	serviceEvent(eventServiceNewPIDs);
}

PMT *eDVB::getPMT()
{
	return tPMT.ready()?tPMT.getCurrent():0;
}

EIT *eDVB::getEIT()
{
	return tEIT.ready()?tEIT.getCurrent():0;
}

struct sortinChannel: public std::unary_function<const eService&, void>
{
	eDVB &edvb;
	sortinChannel(eDVB &edvb): edvb(edvb)
	{
	}
	void operator()(eService &service)
	{
		std::string add;
		switch (service.service_type)
		{
		case 1:
		case 4:
		case 5:
			add=" [TV]";
			break;
		case 2:
			add=" [Radio]";
			break;
		default:
			add=" [Data]";
		}
		eBouquet *b = edvb.createBouquet(beautifyBouquetName(service.service_provider.c_str())+add);
		b->add(service.transport_stream_id, service.original_network_id, service.service_id);
	}
};

void eDVB::sortInChannels()
{
	printf("sorting in channels\n");
	removeDVBBouquets();
	getTransponders()->forEachService(sortinChannel(*this));
	revalidateBouquets();
	saveBouquets();
}

struct saveService: public std::unary_function<const eService&, void>
{
	FILE *f;
	saveService(FILE *out): f(out)
	{
		fprintf(f, "services\n");
	}
	void operator()(eService& s)
	{
		fprintf(f, "%04x:%04x:%04x:%d:%d\n", s.service_id, s.transport_stream_id,
				s.original_network_id, s.service_type, s.service_number);
		fprintf(f, "%s\n", s.service_name.c_str());
		fprintf(f, "%s\n", s.service_provider.c_str());
	}
	~saveService()
	{
		fprintf(f, "end\n");
	}
};

struct saveTransponder: public std::unary_function<const eTransponder&, void>
{
	FILE *f;
	saveTransponder(FILE *out): f(out)
	{
		fprintf(f, "transponders\n");
	}
	void operator()(eTransponder &t)
	{
		if (t.state!=eTransponder::stateOK)
			return;
		fprintf(f, "%04x:%04x %d\n", t.transport_stream_id, t.original_network_id, t.state);
		if (t.cable.valid)
			fprintf(f, "\tc %d:%d\n", t.cable.frequency, t.cable.symbol_rate);
		if (t.satellite.valid)
			fprintf(f, "\ts %d:%d:%d:%d:%d\n", t.satellite.frequency, t.satellite.symbol_rate, t.satellite.polarisation, t.satellite.fec, t.satellite.sat);
		fprintf(f, "/\n");
	}
	~saveTransponder()
	{
		fprintf(f, "end\n");
	}
};

void eDVB::saveServices()
{
	FILE *f=fopen(CONFIGDIR "/enigma/services", "wt");
	if (!f)
		qFatal("couldn't open servicefile - create " CONFIGDIR "/enigma!");
	fprintf(f, "eDVB services - modify as long as you pay for the damage!\n");

	getTransponders()->forEachTransponder(saveTransponder(f));
	getTransponders()->forEachService(saveService(f));
	fprintf(f, "Have a lot of fun!\n");
	fclose(f);
}

void eDVB::loadServices()
{
	FILE *f=fopen(CONFIGDIR "/enigma/services", "rt");
	if (!f)
		return;
	char line[256];
	if ((!fgets(line, 256, f)) || strncmp(line, "eDVB services", 13))
	{
		printf("not a servicefile\n");
		return;
	}
	printf("reading services\n");
	if ((!fgets(line, 256, f)) || strcmp(line, "transponders\n"))
	{
		printf("services invalid, no transponders\n");
		return;
	}
	if (transponderlist)
		delete transponderlist;
	transponderlist=new eTransponderList;

	while (!feof(f))
	{
		if (!fgets(line, 256, f))
			break;
		if (!strcmp(line, "end\n"))
			break;
		int transport_stream_id=-1, original_network_id=-1, state=-1;
		sscanf(line, "%04x:%04x %d", &transport_stream_id, &original_network_id, &state);
		eTransponder &t=transponderlist->createTransponder(transport_stream_id, original_network_id);
		t.state=state;
		while (!feof(f))
		{
			fgets(line, 256, f);
			if (!strcmp(line, "/\n"))
				break;
			if (line[1]=='s')
			{
				int frequency, symbol_rate, polarisation, fec, sat;
				sscanf(line+2, "%d:%d:%d:%d:%d", &frequency, &symbol_rate, &polarisation, &fec, &sat);
				t.setSatellite(frequency, symbol_rate, polarisation, fec, sat);
			}
			if (line[1]=='c')
			{
				int frequency, symbol_rate;
				sscanf(line+2, "%d:%d", &frequency, &symbol_rate);
				t.setCable(frequency, symbol_rate);
			}
		}
	}

	if ((!fgets(line, 256, f)) || strcmp(line, "services\n"))
	{
		printf("services invalid, no services\n");
		return;
	}
	
	int count=0;

	while (!feof(f))
	{
		if (!fgets(line, 256, f))
			break;
		if (!strcmp(line, "end\n"))
			break;

		int service_id=-1, transport_stream_id=-1, original_network_id=-1, service_type=-1, service_number=-1;
		sscanf(line, "%04x:%04x:%04x:%d:%d", &service_id, &transport_stream_id, &original_network_id, &service_type, &service_number);
		eService &s=transponderlist->createService(transport_stream_id, original_network_id, service_id, service_number);
		count++;
		s.service_type=service_type;
		fgets(line, 256, f);
		if (strlen(line))
			line[strlen(line)-1]=0;
		s.service_name=line;
		fgets(line, 256, f);
		if (strlen(line))
			line[strlen(line)-1]=0;
		s.service_provider=line;
	}
	
	printf("loaded %d services\n", count);
	
	fclose(f);
}

void eDVB::saveBouquets()
{
	printf("saving bouquets...\n");
	
	FILE *f=fopen(CONFIGDIR "/enigma/bouquets", "wt");
	if (!f)
		qFatal("couldn't open bouquetfile - create " CONFIGDIR "/enigma!");
	fprintf(f, "eDVB bouquets - modify as long as you don't blame me!\n");
	fprintf(f, "bouquets\n");
	for (BouquetIterator i = (*getBouquets()).begin(); i != (*getBouquets()).end(); ++i)
	{
		eBouquet *b=*i;
		fprintf(f, "%0d\n", b->bouquet_id);
		fprintf(f, "%s\n", b->bouquet_name.c_str());
		for (ServiceReferenceIterator s = b->list.begin(); s != b->list.end(); s++)
			fprintf(f, "%04x:%04x:%04x\n", (*s)->service_id, (*s)->transport_stream_id, (*s)->original_network_id);
		fprintf(f, "/\n");
	}
	fprintf(f, "end\n");
	fprintf(f, "Have a lot of fun!\n");
	fclose(f);
	printf("done\n");
}

void eDVB::loadBouquets()
{
	FILE *f=fopen(CONFIGDIR "/enigma/bouquets", "rt");
	if (!f)
		return;
	char line[256];
	if ((!fgets(line, 256, f)) || strncmp(line, "eDVB bouquets", 13))
	{
		printf("not a bouquetfile\n");
		return;
	}
	printf("reading bouquets\n");
	if ((!fgets(line, 256, f)) || strcmp(line, "bouquets\n"))
	{
		printf("settings invalid, no transponders\n");
		return;
	}

	bouquets.clear();

	while (!feof(f))
	{
		if (!fgets(line, 256, f))
			break;
		if (!strcmp(line, "end\n"))
			break;
		int bouquet_id=-1;
		sscanf(line, "%d", &bouquet_id);
		if (!fgets(line, 256, f))
			break;
		line[strlen(line)-1]=0;
		eBouquet *bouquet=createBouquet(bouquet_id, line);
		while (!feof(f))
		{
			fgets(line, 256, f);
			if (!strcmp(line, "/\n"))
				break;
			int service_id=-1, transport_stream_id=-1, original_network_id=-1;
			sscanf(line, "%04x:%04x:%04x", &service_id, &transport_stream_id, &original_network_id);
			bouquet->add(transport_stream_id, original_network_id, service_id);
		}
	}

	printf("loaded %d bouquets\n", getBouquets()->size());
	
	fclose(f);
	
	revalidateBouquets();
	printf("ok\n");
}

void eDVB::changeVolume(int abs, int vol)
{
	switch (abs)
	{
	case 0:
		volume+=vol;
		mute=0;
		break;
	case 1:
		volume=vol;
		mute=0;
		break;
	case 2:
		if (vol)
			mute=!mute;
		break;
	case 3:
		mute=vol;
		break;
	}
	
	if (volume<0)
		volume=0;
	if (volume>63)
		volume=63;

	eAVSwitch::getInstance()->setVolume(mute?0:((63-volume)*65536/64));

	emit volumeChanged(mute?63:volume);
}

static void unpack(__u32 l, int *t)
{
	for (int i=0; i<4; i++)
		*t++=(l>>((3-i)*8))&0xFF;
}

void eDVB::configureNetwork()
{
	__u32 sip=0, snetmask=0, sdns=0, sgateway=0;
	int ip[4], netmask[4], dns[4], gateway[4];
	int sdosetup=0;

	eDVB::getInstance()->config.getKey("/elitedvb/network/ip", sip);
	eDVB::getInstance()->config.getKey("/elitedvb/network/netmask", snetmask);
	eDVB::getInstance()->config.getKey("/elitedvb/network/dns", sdns);
	eDVB::getInstance()->config.getKey("/elitedvb/network/gateway", sgateway);
	eDVB::getInstance()->config.getKey("/elitedvb/network/dosetup", sdosetup);

	unpack(sip, ip);
	unpack(snetmask, netmask);
	unpack(sdns, dns);
	unpack(sgateway, gateway);
	
	if (sdosetup)
	{
		FILE *f=fopen("/etc/resolv.conf", "wt");
		if (!f)
			printf("couldn't write resolv.conf\n");
		else
		{
			fprintf(f, "# Generated by enigma\nnameserver %d.%d.%d.%d\n", dns[0], dns[1], dns[2], dns[3]);
			fclose(f);
		}
		QString buffer;
		buffer.sprintf("/sbin/ifconfig eth0 %d.%d.%d.%d up netmask %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3], netmask[0], netmask[1], netmask[2], netmask[3]);
		if (system(buffer)>>8)
			printf("'%s' failed.\n", (const char*)buffer);
		else
		{
			system("/bin/route del default 2> /dev/null");
			buffer.sprintf("/bin/route add default gw %d.%d.%d.%d", gateway[0], gateway[1], gateway[2], gateway[3]);
			if (system(buffer)>>8)
				printf("'%s' failed\n", (const char*)buffer);
		}
	}
}

eAutoInitP0<eDVB> init_dvb(4, "eDVB core");
