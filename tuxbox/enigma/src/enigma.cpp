#include <errno.h>
#include <time.h>
#include <malloc.h>
#include <memory.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>
#include <signal.h>
#include <sys/klog.h>

#include <lib/base/i18n.h>
#include <lib/driver/rc.h>
#include <lib/dvb/service.h>
#include <lib/dvb/dvb.h>
#include <lib/dvb/edvb.h>
#include <lib/gdi/gfbdc.h>
#include <lib/gdi/glcddc.h>
#include <lib/gui/emessage.h>
#include <lib/gui/actions.h>
#include <lib/system/econfig.h>
#include <lib/system/httpd.h>
#include <lib/system/http_file.h>
#include <lib/system/http_dyn.h>
#include <lib/system/init.h>
#include <lib/system/init_num.h>
#include <lib/gui/ebutton.h>
#include <lib/gui/actions.h>
#include <lib/driver/rc.h>
#include <lib/dvb/dvbservice.h>
#include <lib/dvb/decoder.h>

#include <lib/system/xmlrpc.h>
#include <enigma.h>
#include <enigma_dyn.h>
#include <enigma_xmlrpc.h>
#include <enigma_main.h>
#include <setup_timezone.h>

// #include <mcheck.h>

eZap *eZap::instance;

static char copyright[]="enigma, Copyright (C) dbox-Project\n"
"enigma comes with ABSOLUTELY NO WARRANTY\n"
"This is free software, and you are welcome\n"
"to redistribute it under certain conditions.\n"
"It is licensed under the GNU General Public License,\n"
"Version 2\n";

eZap *eZap::getInstance()
{
	return instance;
}

void eZap::keyEvent(const eRCKey &key)
{
	if (focus)
		focus->event(eWidgetEvent(eWidgetEvent::evtKey, key));
	else if (main)
		main->event(eWidgetEvent(eWidgetEvent::evtKey, key));
}

void eZap::status()
{
}

#include <lib/base/ringbuffer.h>

extern void ezapInitializeWeb(eHTTPD *httpd, eHTTPDynPathResolver *dyn_resolver);

eZap::eZap(int argc, char **argv)
	: eApplication(/*argc, argv, 0*/)
{
	int bootcount;

#ifndef DISABLE_LCD
	eZapLCD *pLCD;
#endif
	eHTTPDynPathResolver *dyn_resolver;
	eHTTPFilePathResolver *fileresolver;

	instance = this;

	init = new eInit();
	init->setRunlevel(eAutoInitNumbers::osd);

	focus = 0;
	CONNECT(eRCInput::getInstance()->keyEvent, eZap::keyEvent);

	desktop_fb=new eWidget();
	desktop_fb->setName("desktop_fb");
	desktop_fb->move(ePoint(0, 0));
	desktop_fb->resize(eSize(720, 576));
	desktop_fb->setTarget(gFBDC::getInstance());
	desktop_fb->makeRoot();
	desktop_fb->setBackgroundColor(gColor(0));
	desktop_fb->show();
#ifndef DISABLE_LCD
	desktop_lcd=new eWidget();
	desktop_lcd->setName("desktop_lcd");
	desktop_lcd->move(ePoint(0, 0));
	desktop_lcd->resize(eSize(128, 64));
	desktop_lcd->setTarget(gLCDDC::getInstance());
	desktop_lcd->setBackgroundColor(gColor(0));
	desktop_lcd->show();
#endif
	eDebug("[ENIGMA] loading default keymaps...");

#ifndef DISABLE_DREAMBOX_RC
	if ( eActionMapList::getInstance()->loadXML( CONFIGDIR "/enigma/resources/rcdreambox2.xml") )
		eActionMapList::getInstance()->loadXML( DATADIR "/enigma/resources/rcdreambox2.xml");
#endif

#ifndef DISABLE_DBOX_RC
	if ( eActionMapList::getInstance()->loadXML( CONFIGDIR "/enigma/resources/rcdboxold.xml") )
		eActionMapList::getInstance()->loadXML( DATADIR "/enigma/resources/rcdboxold.xml");

	if ( eActionMapList::getInstance()->loadXML( CONFIGDIR "/enigma/resources/rcdboxnew.xml") )
		eActionMapList::getInstance()->loadXML( DATADIR "/enigma/resources/rcdboxnew.xml");

	if ( eActionMapList::getInstance()->loadXML( CONFIGDIR "/enigma/resources/rcdboxbuttons.xml") )
		eActionMapList::getInstance()->loadXML( DATADIR "/enigma/resources/rcdboxbuttons.xml");
#endif

	char *language=0;
	if (eConfig::getInstance()->getKey("/elitedvb/language", language))
		language=strdup("");
	setlocale(LC_ALL, language);
	free(language);

	eDVB::getInstance()->configureNetwork();
	eDebug("<-- network");

	// build Service Selector
	serviceSelector = new eServiceSelector();
	eDebug("<-- service selector");	

	main = new eZapMain();
	eDebug("<-- eZapMain");

#ifndef DISABLE_LCD
	pLCD = eZapLCD::getInstance();
	eDebug("<-- pLCD");
	serviceSelector->setLCD(pLCD->lcdMenu->Title, pLCD->lcdMenu->Element);
#endif

	dyn_resolver = new eHTTPDynPathResolver();
	ezapInitializeDyn(dyn_resolver);

	fileresolver = new eHTTPFilePathResolver();
	fileresolver->addTranslation("/var/tuxbox/htdocs", "/www", 2); /* TODO: make user configurable */
	fileresolver->addTranslation(CONFIGDIR , "/config", 3);
	fileresolver->addTranslation("/", "/root", 3);
	fileresolver->addTranslation(DATADIR "/enigma/htdocs", "/", 2);

	eDebug("[ENIGMA] starting httpd");
	httpd = new eHTTPD(80, eApp);
	ezapInitializeWeb(httpd, dyn_resolver);	

	serialhttpd=0;

	bool SerialConsoleActivated=true;
	FILE *f=fopen("/proc/cmdline", "rt");
	if (f)
	{
		char *cmdline=NULL;
		size_t len = 0;
		getline( &cmdline, &len, f );
		SerialConsoleActivated = strstr( cmdline, "console=ttyS0" ) != NULL;
		fclose(f);
		free(cmdline);
		if ( SerialConsoleActivated )
			eDebug("console=ttyS0 detected...disable enigma serial http interface");
		else
			eDebug("activate enigma serial http interface");
	}

	if ( !SerialConsoleActivated && eDVB::getInstance()->getmID() > 4 )
	{
		eDebug("[ENIGMA] starting httpd on serial port...");
		int fd=::open("/dev/tts/0", O_RDWR);

		if (fd < 0)
			eDebug("[ENIGMA] serial port error (%m)");
		else
		{
			struct termios tio;
			bzero(&tio, sizeof(tio));
			tio.c_cflag = B115200 /*| CRTSCTS*/ | CS8 | CLOCAL | CREAD;
			tio.c_iflag = IGNPAR;
			tio.c_oflag = 0;
			tio.c_lflag = 0;
			tio.c_cc[VTIME] = 0;
			tio.c_cc[VMIN] = 1;
			tcflush(fd, TCIFLUSH);
			tcsetattr(fd, TCSANOW, &tio); 

			logOutputConsole=0; // disable enigma logging to console
			klogctl(8, 0, 1); // disable kernel log to serial

			char *banner="Welcome to the enigma serial access.\r\n"
					"you may start a HTTP session now if you send a \"break\".\r\n";
			write(fd, banner, strlen(banner));
			serialhttpd = new eHTTPConnection(fd, 0, httpd, 1);
			char *i="GET /log/debug HTTP/1.0\n\n";
			serialhttpd->inject(i, strlen(i));
		}
	}

	ezapInitializeXMLRPC(httpd);
	httpd->addResolver(dyn_resolver);
	httpd->addResolver(fileresolver);

	eDebug("[ENIGMA] ok, beginning mainloop");

	eZapTimeZoneSetup settimezone;
	settimezone.setTimeZone();

	if (eConfig::getInstance()->getKey("/elitedvb/system/bootCount", bootcount))
	{
		bootcount = 1;
#if 0
		eMessageBox msg(_("Welcome to enigma.\n\n"
											"Please do a transponder scan first.\n(mainmenu > setup > channels > transponder scan)"),
										_("First start of enigma"),
										eMessageBox::btOK|eMessageBox::iconInfo, eMessageBox::btOK );
		msg.show();
		msg.exec();
		msg.hide();
#endif
	}
	else
		bootcount++;

	eConfig::getInstance()->setKey("/elitedvb/system/bootCount", bootcount);

	init->setRunlevel(eAutoInitNumbers::main);
}

eZap::~eZap()
{
	eDebug("[ENIGMA] beginning clean shutdown");
	eDebug("[ENIGMA] main");
	delete main;
	eDebug("[ENIGMA] serviceSelector");
	delete serviceSelector;
	eDebug("[ENIGMA] fertig");
	init->setRunlevel(-1);

  if (serialhttpd)
    delete serialhttpd;
    
	delete httpd;
	delete init;
	instance = 0;
}

void fault(int x)
{
	printf(" ----- segfault :/\n");
	exit(2);
}

extern "C" void __mp_initsection();

int main(int argc, char **argv)
{
	time_t t=0;
	int res;
//	signal(SIGSEGV, fault);
//	printf("(secret data: %x)\n", __mp_initsection);

	stime(&t);
	eDebug("%s", copyright);

	setlocale (LC_ALL, "");
	bindtextdomain ("tuxbox-enigma", "/share/locale");
	bind_textdomain_codeset("tuxbox-enigma", "UTF8");
	textdomain ("tuxbox-enigma");

	Decoder::Initialize();	
	Decoder::displayIFrameFromFile("/iframe");
	
//	mtrace();
//	mcheck(0);
	
	{
		eZap ezap(argc, argv);
		res=ezap.exec();
	}

	Decoder::Flush();

	exit(res);
//	mcheck_check_all();
//	muntrace();
}

extern "C" void mkstemps();
void mkstemps()
{
}
