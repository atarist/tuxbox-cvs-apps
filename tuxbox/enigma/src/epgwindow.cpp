#include "epgwindow.h"
#include "rc.h"
#include "eskin.h"
#include <algorithm>
#include "edvb.h"
#include "enigma_event.h"

eListboxEntryEPG::eListboxEntryEPG(EITEvent* evt, eListbox *listbox): eListboxEntry(listbox), event(evt)
{
}

eListboxEntryEPG::~eListboxEntryEPG()
{
	delete event;
}

QString eListboxEntryEPG::getText(int col=0) const
{
		tm* t = localtime(&event->start_time);
		QString DateTime;
		DateTime.sprintf("%2d.%d, %02d:%02d   ", t->tm_mday, t->tm_mon+1, t->tm_hour, t->tm_min);
		for (QListIterator<Descriptor> d(event->descriptor); d.current(); ++d)
		{
			Descriptor *descriptor=d.current();
			if (descriptor->Tag()==DESCR_SHORT_EVENT)
			{
				ShortEventDescriptor *ss=(ShortEventDescriptor*)descriptor;
				return DateTime+ss->event_name;
			}
		}
		return QString("no_name");
}

void eEPGWindow::fillEPGList()
{
	setText("EPG");
	qDebug("get EventMap for onid: %02x, sid: %02x\n", current->original_network_id, current->service_id);
	const eventMap& evt = eEPGCache::getInstance()->getEventMap(current->original_network_id, current->service_id);
	eventMap::const_iterator It;
	for (It = evt.begin(); It != evt.end(); It++)
		new eListboxEntryEPG(new EITEvent(*It->second) , list);
}

void eEPGWindow::entrySelected(eListboxEntry *entry)
{
	hide();
	eEventDisplay ei(eDVB::getInstance()->service->service_name, 0, ((eListboxEntryEPG*)entry)->event);
	ei.show();
	ei.exec();
	ei.hide();
	show();
}

int eEPGWindow::eventFilter(const eWidgetEvent &event)
{
	switch (event.type)
	{
	case eWidgetEvent::keyDown:
		switch (event.parameter)
		{
			case eRCInput::RC_HOME:
 			close(0);
			break;
		}
	}
	return 0;
}

eEPGWindow::eEPGWindow(eService* service):current(service),
								eLBWindow("Select Service...", eListbox::tLitebar, 16, eSkin::getActive()->queryValue("fontsize", 20), 600)
					
{
	move(QPoint(50, 50));
	list->setActiveColor(eSkin::getActive()->queryScheme("eServiceSelector.highlight"));
	connect(list, SIGNAL(selected(eListboxEntry*)), SLOT(entrySelected(eListboxEntry*)));
	fillEPGList();
}

eEPGWindow::~eEPGWindow()
{
}
