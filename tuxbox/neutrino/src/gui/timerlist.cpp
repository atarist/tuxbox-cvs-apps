/*
	Neutrino-GUI  -   DBoxII-Project

	Timerliste by Zwen
	
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/timerlist.h>
#include <gui/pluginlist.h>
#include <gui/plugins.h>

#include <daemonc/remotecontrol.h>

#include <driver/encoding.h>
#include <driver/fontrenderer.h>
#include <driver/rcinput.h>

#include <gui/color.h>
#include <gui/eventlist.h>
#include <gui/infoviewer.h>

#include <gui/widget/buttons.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/icons.h>
#include <gui/widget/menue.h>
#include <gui/widget/messagebox.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/stringinput_ext.h>

#include <system/settings.h>

#include <global.h>
#include <neutrino.h>

#include <zapit/client/zapitclient.h>
#include <zapit/client/zapittools.h>

#include <string.h>

#define info_height 60


class CTimerListNewNotifier : public CChangeObserver
{
private:
	CMenuItem* m1;
	CMenuItem* m2;
	CMenuItem* m3;
	CMenuItem* m4;
	CMenuItem* m5;
	char* display;
	int* iType;
	time_t* stopTime;
public:
	CTimerListNewNotifier( int* Type, time_t* time,CMenuItem* a1, CMenuItem* a2, 
						   CMenuItem* a3, CMenuItem* a4, CMenuItem* a5, char* d)
	{
		m1 = a1;
		m2 = a2;
		m3 = a3;
		m4 = a4;
		m5 = a5;
		display=d;
		iType=Type;
		stopTime=time;
	}
	bool changeNotify(const neutrino_locale_t OptionName, void *)
	{
		CTimerd::CTimerEventTypes type = (CTimerd::CTimerEventTypes) *iType;
		if(type == CTimerd::TIMER_RECORD)
		{
			*stopTime=(time(NULL)/60)*60;
			struct tm *tmTime2 = localtime(stopTime);
			sprintf( display, "%02d.%02d.%04d %02d:%02d", tmTime2->tm_mday, tmTime2->tm_mon+1,
						tmTime2->tm_year+1900,
						tmTime2->tm_hour, tmTime2->tm_min);
			m1->setActive (true);
		}
		else
		{
			*stopTime=0;
			strcpy(display,"                ");
			m1->setActive (false);
		}
		if(type == CTimerd::TIMER_RECORD ||
			type == CTimerd::TIMER_ZAPTO ||
			type == CTimerd::TIMER_NEXTPROGRAM)
		{
			m2->setActive(true);
		}
		else
		{
			m2->setActive(false);
		}
		if(type == CTimerd::TIMER_STANDBY)
			m3->setActive(true);
		else
			m3->setActive(false);
		if(type == CTimerd::TIMER_REMIND)
			m4->setActive(true);
		else
			m4->setActive(false);
		if(type == CTimerd::TIMER_EXEC_PLUGIN)
			m5->setActive(true);
		else
			m5->setActive(false);
		return true;
	}
};

class CTimerListRepeatNotifier : public CChangeObserver
{
private:
	CMenuForwarder* m;
	int* iRepeat;
public:
	CTimerListRepeatNotifier( int* repeat, CMenuForwarder* a)
	{
		m = a;
		iRepeat=repeat;
	}
	bool changeNotify(const neutrino_locale_t OptionName, void *)
	{
		if(*iRepeat >= (int)CTimerd::TIMERREPEAT_WEEKDAYS)
			m->setActive (true);
		else
			m->setActive (false);
		return true;
	}
};


CTimerList::CTimerList()
{
	frameBuffer = CFrameBuffer::getInstance();
	visible = false;
	selected = 0;
	// Max
	width = 720;
	if(g_settings.screen_EndX-g_settings.screen_StartX < width)
		width=g_settings.screen_EndX-g_settings.screen_StartX-10;
	buttonHeight = 25;
	theight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	fheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	x=(((g_settings.screen_EndX- g_settings.screen_StartX)-width) / 2) + g_settings.screen_StartX;
	y=(((g_settings.screen_EndY- g_settings.screen_StartY)-( height+ info_height) ) / 2) + g_settings.screen_StartY;
	liststart = 0;
	Timer = new CTimerdClient();
	skipEventID=0;
}

CTimerList::~CTimerList()
{
	timerlist.clear();
	delete Timer;
}

int CTimerList::exec(CMenuTarget* parent, const std::string & actionKey)
{
	const char * key = actionKey.c_str();

	if (strcmp(key, "modifytimer") == 0)
	{
		timerlist[selected].announceTime = timerlist[selected].alarmTime -60;
		if(timerlist[selected].eventRepeat >= CTimerd::TIMERREPEAT_WEEKDAYS)
			Timer->getWeekdaysFromStr((int *)&timerlist[selected].eventRepeat, m_weekdaysStr);
		if(timerlist[selected].eventType == CTimerd::TIMER_RECORD)
		{
			timerlist[selected].announceTime -= 120; // 2 more mins for rec timer
			Timer->modifyTimerAPid(timerlist[selected].eventID,timerlist[selected].apids);
		}
		Timer->modifyTimerEvent (timerlist[selected].eventID, timerlist[selected].announceTime, 
								 timerlist[selected].alarmTime, 
								 timerlist[selected].stopTime, timerlist[selected].eventRepeat);
		return menu_return::RETURN_EXIT;
	}
	else if (strcmp(key, "newtimer") == 0)
	{
		timerNew.announceTime=timerNew.alarmTime-60;
		CTimerd::EventInfo eventinfo;
		eventinfo.epgID=0;
		eventinfo.epg_starttime=0;
		eventinfo.channel_id=timerNew.channel_id;
		eventinfo.apids = "";
		eventinfo.recordingSafety = false;
		timerNew.standby_on = (timerNew_standby_on == 1);
		void *data=NULL;
		if(timerNew.eventType == CTimerd::TIMER_STANDBY)
			data=&(timerNew.standby_on);
		else if(timerNew.eventType==CTimerd::TIMER_NEXTPROGRAM || 
				  timerNew.eventType==CTimerd::TIMER_ZAPTO ||
				  timerNew.eventType==CTimerd::TIMER_RECORD)
		{
			if (timerNew.eventType==CTimerd::TIMER_RECORD)
				timerNew.announceTime-= 120; // 2 more mins for rec timer
			if (strcmp(timerNew_channel_name, "---")==0)
				return menu_return::RETURN_REPAINT;
			data= &eventinfo;
		}
		else if(timerNew.eventType==CTimerd::TIMER_REMIND)
			data= timerNew.message;
		else if (timerNew.eventType==CTimerd::TIMER_EXEC_PLUGIN)
		{
			if (strcmp(timerNew.pluginName, "---") == 0)
				return menu_return::RETURN_REPAINT;
			data= timerNew.pluginName;
		}
		if(timerNew.eventRepeat >= CTimerd::TIMERREPEAT_WEEKDAYS)
			Timer->getWeekdaysFromStr((int *)&timerNew.eventRepeat, m_weekdaysStr);
		Timer->addTimerEvent(timerNew.eventType,data,timerNew.announceTime,timerNew.alarmTime,
									timerNew.stopTime,timerNew.eventRepeat);
		return menu_return::RETURN_EXIT;
	}
	else if (strncmp(key, "SC:", 3) == 0)
	{
		int delta;
		sscanf(&(key[3]),
		       SCANF_CHANNEL_ID_TYPE
		       "%n",
		       &timerNew.channel_id,
		       &delta);
#if 0
		strncpy(timerNew_channel_name, &(key[3 + delta + 1]), 30);
#else
		strncpy(timerNew_channel_name, ZapitTools::UTF8_to_Latin1(&(key[3 + delta + 1])).c_str(), 30);
#endif
		g_RCInput->postMsg(CRCInput::RC_timeout, 0); // leave underlying menu also
		g_RCInput->postMsg(CRCInput::RC_timeout, 0); // leave underlying menu also
		return menu_return::RETURN_EXIT;
	}

	if(parent)
	{
		parent->hide();
	}

	int ret = show();

	return ret;
/*
	if( ret > -1)
	{
		return menu_return::RETURN_REPAINT;
	}
	else if( ret == -1)
	{
		// -1 bedeutet nur REPAINT
		return menu_return::RETURN_REPAINT;
	}
	else
	{
		// -2 bedeutet EXIT_ALL
		return menu_return::RETURN_EXIT_ALL;
	}*/
}

void CTimerList::updateEvents(void)
{
	timerlist.clear();
	Timer->getTimerList (timerlist);
	//Remove last deleted event from List
	CTimerd::TimerList::iterator timer = timerlist.begin();
	for(; timer != timerlist.end();timer++)
	{
		if(timer->eventID==skipEventID)
		{
			timerlist.erase(timer);
			break;
		}
	}
	sort(timerlist.begin(), timerlist.end());

	height = (g_settings.screen_EndY-g_settings.screen_StartY)-(info_height+50);
	listmaxshow = (height-theight-0)/(fheight*2);
	height = theight+0+listmaxshow*fheight*2;	// recalc height
	if(timerlist.size() < listmaxshow)
	{
		listmaxshow=timerlist.size();
		height = theight+0+listmaxshow*fheight*2;	// recalc height
	}
	if(selected==timerlist.size() && !(timerlist.empty()))
	{
		selected=timerlist.size()-1;
		liststart = (selected/listmaxshow)*listmaxshow;
	}
	x=(((g_settings.screen_EndX- g_settings.screen_StartX)-width) / 2) + g_settings.screen_StartX;
	y=(((g_settings.screen_EndY- g_settings.screen_StartY)-( height+ info_height) ) / 2) + g_settings.screen_StartY;
}


int CTimerList::show()
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int res = menu_return::RETURN_REPAINT;

	unsigned long long timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

	bool loop=true;
	bool update=true;
	while(loop)
	{
		if(update)
		{
			hide();
			updateEvents();
			update=false;
//			if (timerlist.empty())
//			{
				//evtl. anzeige dass keine kanalliste....
				/* ShowHintUTF(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_TIMERLIST_EMPTY));
				 return -1;*/
//			}
			paint();
		}
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );

		if( msg <= CRCInput::RC_MaxRC )
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

		if( ( msg == CRCInput::RC_timeout ) ||
			 ( msg == CRCInput::RC_home) )
		{ //Exit after timeout or cancel key
			loop=false;
		}
		else if ((msg == CRCInput::RC_up) && !(timerlist.empty()))
		{
			int prevselected=selected;
			if(selected==0)
			{
				selected = timerlist.size()-1;
			}
			else
				selected--;
			paintItem(prevselected - liststart);
			unsigned int oldliststart = liststart;
			liststart = (selected/listmaxshow)*listmaxshow;
			if(oldliststart!=liststart)
			{
				paint();
			}
			else
			{
				paintItem(selected - liststart);
			}
		}
		else if ((msg == CRCInput::RC_down) && !(timerlist.empty()))
		{
			int prevselected=selected;
			selected = (selected+1)%timerlist.size();
			paintItem(prevselected - liststart);
			unsigned int oldliststart = liststart;
			liststart = (selected/listmaxshow)*listmaxshow;
			if(oldliststart!=liststart)
			{
				paint();
			}
			else
			{
				paintItem(selected - liststart);
			}
		}
		else if ((msg == CRCInput::RC_ok) && !(timerlist.empty()))
		{
			if (modifyTimer()==menu_return::RETURN_EXIT_ALL)
			{
				res=menu_return::RETURN_EXIT_ALL;
				loop=false;
			}
			else
				update=true;
		}
		else if((msg == CRCInput::RC_red) && !(timerlist.empty()))
		{
			Timer->removeTimerEvent(timerlist[selected].eventID);
			skipEventID=timerlist[selected].eventID;
			update=true;
		}
		else if(msg==CRCInput::RC_green)
		{
			if (newTimer()==menu_return::RETURN_EXIT_ALL)
			{
				res=menu_return::RETURN_EXIT_ALL;
				loop=false;
			}
			else
				update=true;
		}
		else if(msg==CRCInput::RC_yellow)
		{
			update=true;
		}
		else if((msg==CRCInput::RC_blue)||
				  (CRCInput::isNumeric(msg)) )
		{
			//pushback key if...
			g_RCInput->postMsg( msg, data );
			loop=false;
		}
		else if(msg==CRCInput::RC_setup)
		{
			res=menu_return::RETURN_EXIT_ALL;
			loop=false;
		}
		else if( msg == CRCInput::RC_help )
		{
			CTimerd::responseGetTimer* timer=&timerlist[selected];
			if(timer!=NULL)
			{
				if(timer->eventType == CTimerd::TIMER_RECORD || timer->eventType == CTimerd::TIMER_ZAPTO)
				{
					hide();
					res = g_EpgData->show(timer->channel_id, timer->epgID, &timer->epg_starttime);
					if(res==menu_return::RETURN_EXIT_ALL)
						loop=false;
					else
						paint();
				}
			}
			// help key
		}
		else
		{
			if( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all )
			{
				loop = false;
				res = menu_return::RETURN_EXIT_ALL;
			}
		}
	}
	hide();

	return(res);
}

void CTimerList::hide()
{
	if(visible)
	{
		frameBuffer->paintBackgroundBoxRel(x, y, width, height+ info_height+ 5);
		visible = false;
	}
}

void CTimerList::paintItem(int pos)
{
	int ypos = y+ theight+0 + pos*fheight*2;

	uint8_t    color;
	fb_pixel_t bgcolor;

	if (pos & 1)
	{
		color   = COL_MENUCONTENTDARK;
		bgcolor = COL_MENUCONTENTDARK_PLUS_0;
	}
	else
	{
		color   = COL_MENUCONTENT;
		bgcolor = COL_MENUCONTENT_PLUS_0;
	}

	if (liststart + pos == selected)
	{
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}

	int real_width=width;
	if(timerlist.size()>listmaxshow)
	{
		real_width-=15; //scrollbar
	}
	
	frameBuffer->paintBoxRel(x,ypos, real_width, 2*fheight, bgcolor);
	if(liststart+pos<timerlist.size())
	{
		CTimerd::responseGetTimer & timer = timerlist[liststart+pos];
		char zAlarmTime[25] = {0};
		struct tm *alarmTime = localtime(&(timer.alarmTime));
		strftime(zAlarmTime,20,"%d.%m. %H:%M",alarmTime);
		char zStopTime[25] = {0};
		struct tm *stopTime = localtime(&(timer.stopTime));
		strftime(zStopTime,20,"%d.%m. %H:%M",stopTime);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+10,ypos+fheight, 150, zAlarmTime, color, fheight, true); // UTF-8
		if(timer.stopTime != 0)
		{
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+10,ypos+2*fheight, 150, zStopTime, color, fheight, true); // UTF-8
		}
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+160,ypos+fheight, (real_width-160)/2-5, convertTimerRepeat2String(timer.eventRepeat), color, fheight, true); // UTF-8
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+160+(real_width-160)/2,ypos+fheight, (real_width-160)/2-5, convertTimerType2String(timer.eventType), color, fheight, true); // UTF-8
		std::string zAddData("");
		switch(timer.eventType)
		{
			case CTimerd::TIMER_NEXTPROGRAM :
			case CTimerd::TIMER_ZAPTO :
			case CTimerd::TIMER_RECORD :
				{
					zAddData = convertChannelId2String(timer.channel_id); // UTF-8
					if(strlen(timer.apids) != 0)
					{
						zAddData += " (";
						zAddData += timer.apids; // must be UTF-8 encoded !
						zAddData += ')';
					}
					if(timer.epgID!=0)
					{
						CEPGData epgdata;
						if (g_Sectionsd->getEPGid(timer.epgID, timer.epg_starttime, &epgdata))
						{
#warning fixme sectionsd should deliver data in UTF-8 format
							zAddData += " : ";
							zAddData += Latin1_to_UTF8(epgdata.title);
						}
					}
				}
				break;
			case CTimerd::TIMER_STANDBY:
				{
					zAddData = g_Locale->getText(timer.standby_on ? LOCALE_TIMERLIST_STANDBY_ON : LOCALE_TIMERLIST_STANDBY_OFF);
					break;
				}
			case CTimerd::TIMER_REMIND :
				{
					zAddData = timer.message; // must be UTF-8 encoded !
				}
				break;
			case CTimerd::TIMER_EXEC_PLUGIN :
			{
				zAddData = timer.pluginName;
			}
			break;
			default:{}
		}
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+160,ypos+2*fheight, real_width-165, zAddData, color, fheight, true); // UTF-8
		// LCD Display
		if(liststart+pos==selected)
		{
			std::string line1 = convertTimerType2String(timer.eventType); // UTF-8
			std::string line2 = zAlarmTime;
			switch(timer.eventType)
			{
				case CTimerd::TIMER_RECORD :
					line2+= " -";
					line2+= zStopTime+6;
				case CTimerd::TIMER_NEXTPROGRAM :
				case CTimerd::TIMER_ZAPTO :
					{
						line1 += ' ';
						line1 += convertChannelId2String(timer.channel_id); // UTF-8
					}
					break;
				case CTimerd::TIMER_STANDBY :
					{
						if(timer.standby_on)
							line1+=" ON";
						else
							line1+=" OFF";
					}
					break;
			default:;
			}
			CLCD::getInstance()->showMenuText(0, line1.c_str(), -1, true); // UTF-8
			CLCD::getInstance()->showMenuText(1, line2.c_str(), -1, true); // UTF-8
		}
	}
}

void CTimerList::paintHead()
{
	frameBuffer->paintBoxRel(x,y, width,theight+0, COL_MENUHEAD_PLUS_0);
	frameBuffer->paintIcon("timer.raw",x+5,y+4);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(x+35,y+theight+0, width- 45, g_Locale->getText(LOCALE_TIMERLIST_NAME), COL_MENUHEAD, 0, true); // UTF-8

	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_HELP, x+ width- 30, y+ 5 );
/*	if (bouquetList!=NULL)
		frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_DBOX, x+ width- 60, y+ 5 );*/
}

const struct button_label TimerListButtons[3] =
{
	{ NEUTRINO_ICON_BUTTON_RED   , LOCALE_TIMERLIST_DELETE },
	{ NEUTRINO_ICON_BUTTON_GREEN , LOCALE_TIMERLIST_NEW    },
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_TIMERLIST_RELOAD }
};

void CTimerList::paintFoot()
{
	int ButtonWidth = (width - 20) / 4;
	frameBuffer->paintBoxRel(x,y+height, width,buttonHeight, COL_MENUHEAD_PLUS_0);
	frameBuffer->paintHLine(x, x+width,  y, COL_INFOBAR_SHADOW_PLUS_0);

	if (timerlist.empty())
		::paintButtons(frameBuffer, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL], g_Locale, x + ButtonWidth + 10, y + height + 4, ButtonWidth, 2, &(TimerListButtons[1]));
	else
	{
		::paintButtons(frameBuffer, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL], g_Locale, x + 10, y + height + 4, ButtonWidth, 3, TimerListButtons);

		frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_OKAY, x+width- 1* ButtonWidth + 10, y+height);
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(x+width-1 * ButtonWidth + 38, y+height+24 - 2, ButtonWidth- 28, g_Locale->getText(LOCALE_TIMERLIST_MODIFY), COL_INFOBAR, 0, true); // UTF-8
	}
}

void CTimerList::paint()
{
	unsigned int page_nr = (listmaxshow == 0) ? 0 : (selected / listmaxshow);
	liststart = page_nr * listmaxshow;

	CLCD::getInstance()->setMode(CLCD::MODE_MENU_UTF8, g_Locale->getText(LOCALE_TIMERLIST_NAME));

	paintHead();
	for(unsigned int count=0;count<listmaxshow;count++)
	{
		paintItem(count);
	}

	if(timerlist.size()>listmaxshow)
	{
		int ypos = y+ theight;
		int sb = 2*fheight* listmaxshow;
		frameBuffer->paintBoxRel(x+ width- 15,ypos, 15, sb, COL_MENUCONTENT_PLUS_1);

		int sbc= ((timerlist.size()- 1)/ listmaxshow)+ 1;
		float sbh= (sb- 4)/ sbc;

		frameBuffer->paintBoxRel(x+ width- 13, ypos+ 2+ int(page_nr * sbh) , 11, int(sbh), COL_MENUCONTENT_PLUS_3);
	}

	paintFoot();
	visible = true;
}

const char * CTimerList::convertTimerType2String(const CTimerd::CTimerEventTypes type) // UTF-8
{
	switch(type)
	{
		case CTimerd::TIMER_SHUTDOWN    : return g_Locale->getText(LOCALE_TIMERLIST_TYPE_SHUTDOWN   );
		case CTimerd::TIMER_NEXTPROGRAM : return g_Locale->getText(LOCALE_TIMERLIST_TYPE_NEXTPROGRAM);
		case CTimerd::TIMER_ZAPTO       : return g_Locale->getText(LOCALE_TIMERLIST_TYPE_ZAPTO      );
		case CTimerd::TIMER_STANDBY     : return g_Locale->getText(LOCALE_TIMERLIST_TYPE_STANDBY    );
		case CTimerd::TIMER_RECORD      : return g_Locale->getText(LOCALE_TIMERLIST_TYPE_RECORD     );
		case CTimerd::TIMER_REMIND      : return g_Locale->getText(LOCALE_TIMERLIST_TYPE_REMIND     );
		case CTimerd::TIMER_SLEEPTIMER  : return g_Locale->getText(LOCALE_TIMERLIST_TYPE_SLEEPTIMER );
		case CTimerd::TIMER_EXEC_PLUGIN : return g_Locale->getText(LOCALE_TIMERLIST_TYPE_EXECPLUGIN );
		default                         : return g_Locale->getText(LOCALE_TIMERLIST_TYPE_UNKNOWN    );
	}
}

std::string CTimerList::convertTimerRepeat2String(const CTimerd::CTimerEventRepeat rep) // UTF-8
{
	switch(rep)
	{
		case CTimerd::TIMERREPEAT_ONCE               : return g_Locale->getText(LOCALE_TIMERLIST_REPEAT_ONCE              );
		case CTimerd::TIMERREPEAT_DAILY              : return g_Locale->getText(LOCALE_TIMERLIST_REPEAT_DAILY             );
		case CTimerd::TIMERREPEAT_WEEKLY             : return g_Locale->getText(LOCALE_TIMERLIST_REPEAT_WEEKLY            );
		case CTimerd::TIMERREPEAT_BIWEEKLY           : return g_Locale->getText(LOCALE_TIMERLIST_REPEAT_BIWEEKLY          );
		case CTimerd::TIMERREPEAT_FOURWEEKLY         : return g_Locale->getText(LOCALE_TIMERLIST_REPEAT_FOURWEEKLY        );
		case CTimerd::TIMERREPEAT_MONTHLY            : return g_Locale->getText(LOCALE_TIMERLIST_REPEAT_MONTHLY           );
		case CTimerd::TIMERREPEAT_BYEVENTDESCRIPTION : return g_Locale->getText(LOCALE_TIMERLIST_REPEAT_BYEVENTDESCRIPTION);
		default: 
			if(rep >=CTimerd::TIMERREPEAT_WEEKDAYS)
			{
				int weekdays = (((int)rep) >> 9);
				std::string weekdayStr="";
				if(weekdays & 1)
					weekdayStr+= g_Locale->getText(LOCALE_TIMERLIST_REPEAT_MONDAY);
				weekdays >>= 1;
				if(weekdays & 1)
					weekdayStr+= g_Locale->getText(LOCALE_TIMERLIST_REPEAT_TUESDAY);
				weekdays >>= 1;
				if(weekdays & 1)
					weekdayStr+= g_Locale->getText(LOCALE_TIMERLIST_REPEAT_WEDNESDAY);
				weekdays >>= 1;
				if(weekdays & 1)
					weekdayStr+= g_Locale->getText(LOCALE_TIMERLIST_REPEAT_THURSDAY);
				weekdays >>= 1;
				if(weekdays & 1)
					weekdayStr+= g_Locale->getText(LOCALE_TIMERLIST_REPEAT_FRIDAY);
				weekdays >>= 1;
				if(weekdays & 1)
					weekdayStr+= g_Locale->getText(LOCALE_TIMERLIST_REPEAT_SATURDAY);
				weekdays >>= 1;
				if(weekdays & 1)
					weekdayStr+= g_Locale->getText(LOCALE_TIMERLIST_REPEAT_SUNDAY);
				return weekdayStr;
			}
			else
				return g_Locale->getText(LOCALE_TIMERLIST_REPEAT_UNKNOWN);
	}
}

std::string CTimerList::convertChannelId2String(const t_channel_id id) // UTF-8
{
	CZapitClient Zapit;
	std::string name = Zapit.getChannelName(id); // UTF-8
	if (name.empty())
		name = g_Locale->getText(LOCALE_TIMERLIST_PROGRAM_UNKNOWN);
   
	return name;
}

#define TIMERLIST_REPEAT_OPTION_COUNT 7
const CMenuOptionChooser::keyval TIMERLIST_REPEAT_OPTIONS[TIMERLIST_REPEAT_OPTION_COUNT] =
{
	{ CTimerd::TIMERREPEAT_ONCE       , LOCALE_TIMERLIST_REPEAT_ONCE       },
	{ CTimerd::TIMERREPEAT_DAILY      , LOCALE_TIMERLIST_REPEAT_DAILY      },
	{ CTimerd::TIMERREPEAT_WEEKLY     , LOCALE_TIMERLIST_REPEAT_WEEKLY     },
	{ CTimerd::TIMERREPEAT_BIWEEKLY   , LOCALE_TIMERLIST_REPEAT_BIWEEKLY   },
	{ CTimerd::TIMERREPEAT_FOURWEEKLY , LOCALE_TIMERLIST_REPEAT_FOURWEEKLY },
	{ CTimerd::TIMERREPEAT_MONTHLY    , LOCALE_TIMERLIST_REPEAT_MONTHLY    },
	{ CTimerd::TIMERREPEAT_WEEKDAYS   , LOCALE_TIMERLIST_REPEAT_WEEKDAYS   }
};

#define TIMERLIST_STANDBY_OPTION_COUNT 2
const CMenuOptionChooser::keyval TIMERLIST_STANDBY_OPTIONS[TIMERLIST_STANDBY_OPTION_COUNT] =
{
	{ 0 , LOCALE_TIMERLIST_STANDBY_OFF },
	{ 1 , LOCALE_TIMERLIST_STANDBY_ON  }
};

#if 1
#define TIMERLIST_TYPE_OPTION_COUNT 7
#else
#define TIMERLIST_TYPE_OPTION_COUNT 8
#endif
const CMenuOptionChooser::keyval TIMERLIST_TYPE_OPTIONS[TIMERLIST_TYPE_OPTION_COUNT] =
{
	{ CTimerd::TIMER_SHUTDOWN   , LOCALE_TIMERLIST_TYPE_SHUTDOWN    },
#if 0
	{ CTimerd::TIMER_NEXTPROGRAM, LOCALE_TIMERLIST_TYPE_NEXTPROGRAM },
#endif
	{ CTimerd::TIMER_ZAPTO      , LOCALE_TIMERLIST_TYPE_ZAPTO       },
	{ CTimerd::TIMER_STANDBY    , LOCALE_TIMERLIST_TYPE_STANDBY     },
	{ CTimerd::TIMER_RECORD     , LOCALE_TIMERLIST_TYPE_RECORD      },
	{ CTimerd::TIMER_SLEEPTIMER , LOCALE_TIMERLIST_TYPE_SLEEPTIMER  },
	{ CTimerd::TIMER_REMIND     , LOCALE_TIMERLIST_TYPE_REMIND      },
	{ CTimerd::TIMER_EXEC_PLUGIN, LOCALE_TIMERLIST_TYPE_EXECPLUGIN  }
};

int CTimerList::modifyTimer()
{
	CTimerd::responseGetTimer* timer=&timerlist[selected];
	CMenuWidget timerSettings(LOCALE_TIMERLIST_MENUMODIFY, NEUTRINO_ICON_SETTINGS);
	timerSettings.addItem(GenericMenuSeparator);
	timerSettings.addItem(GenericMenuBack);
	timerSettings.addItem(GenericMenuSeparatorLine);

	char type[80];
	strcpy(type, ZapitTools::UTF8_to_Latin1(convertTimerType2String(timer->eventType)).c_str()); // UTF8, UTF8 -> Latin1
	CMenuForwarder *m0 = new CMenuForwarder(LOCALE_TIMERLIST_TYPE, false, type);
	timerSettings.addItem( m0);

	CDateInput timerSettings_alarmTime(LOCALE_TIMERLIST_ALARMTIME, &timer->alarmTime , LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	CMenuForwarder *m1 = new CMenuForwarder(LOCALE_TIMERLIST_ALARMTIME, true, timerSettings_alarmTime.getValue (), &timerSettings_alarmTime );
	timerSettings.addItem( m1);

	CDateInput timerSettings_stopTime(LOCALE_TIMERLIST_STOPTIME, &timer->stopTime , LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	if(timer->stopTime != 0)
	{
		CMenuForwarder *m2 = new CMenuForwarder(LOCALE_TIMERLIST_STOPTIME, true, timerSettings_stopTime.getValue (), &timerSettings_stopTime );
		timerSettings.addItem( m2);
	}

	Timer->setWeekdaysToStr(timer->eventRepeat, m_weekdaysStr);
	timer->eventRepeat = (CTimerd::CTimerEventRepeat)(((int)timer->eventRepeat) & 0x1FF);
	CStringInput timerSettings_weekdays(LOCALE_TIMERLIST_WEEKDAYS, m_weekdaysStr, 7, LOCALE_TIMERLIST_WEEKDAYS_HINT_1, LOCALE_TIMERLIST_WEEKDAYS_HINT_2, "-X");
	CMenuForwarder *m4 = new CMenuForwarder(LOCALE_TIMERLIST_WEEKDAYS, ((int)timer->eventRepeat) >= (int)CTimerd::TIMERREPEAT_WEEKDAYS, m_weekdaysStr, &timerSettings_weekdays );
	CTimerListRepeatNotifier notifier((int *)&timer->eventRepeat,m4);
	CMenuOptionChooser* m3 = new CMenuOptionChooser(LOCALE_TIMERLIST_REPEAT, (int *)&timer->eventRepeat, TIMERLIST_REPEAT_OPTIONS, TIMERLIST_REPEAT_OPTION_COUNT, true, &notifier);

	timerSettings.addItem(m3);
	timerSettings.addItem(m4);

	CStringInput timerSettings_apids(LOCALE_TIMERLIST_APIDS, timer->apids , 25, LOCALE_APIDS_HINT_1, LOCALE_APIDS_HINT_2, "0123456789ABCDEF ");
	if(timer->eventType ==  CTimerd::TIMER_RECORD)
	{
		CMenuForwarder *m5 = new CMenuForwarder(LOCALE_TIMERLIST_APIDS, true, timer->apids, &timerSettings_apids );
		timerSettings.addItem( m5);
	}

	timerSettings.addItem(new CMenuForwarder(LOCALE_TIMERLIST_SAVE, true, NULL, this, "modifytimer"));

	return timerSettings.exec(this,"");
}

int CTimerList::newTimer()
{
	std::vector<CMenuWidget *> toDelete;
	// Defaults
	timerNew.eventType = CTimerd::TIMER_SHUTDOWN ;
	timerNew.eventRepeat = CTimerd::TIMERREPEAT_ONCE ;
	timerNew.alarmTime = (time(NULL)/60)*60;
	timerNew.stopTime = 0;
	timerNew.channel_id = 0;
	strcpy(timerNew.message, "");
	timerNew_standby_on =false;

	CMenuWidget timerSettings(LOCALE_TIMERLIST_MENUNEW, NEUTRINO_ICON_SETTINGS);
	timerSettings.addItem(GenericMenuSeparator);
	timerSettings.addItem(GenericMenuBack);
	timerSettings.addItem(GenericMenuSeparatorLine);

	CDateInput timerSettings_alarmTime(LOCALE_TIMERLIST_ALARMTIME, &(timerNew.alarmTime) , LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	CMenuForwarder *m1 = new CMenuForwarder(LOCALE_TIMERLIST_ALARMTIME, true, timerSettings_alarmTime.getValue (), &timerSettings_alarmTime );

	CDateInput timerSettings_stopTime(LOCALE_TIMERLIST_STOPTIME, &(timerNew.stopTime) , LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	CMenuForwarder *m2 = new CMenuForwarder(LOCALE_TIMERLIST_STOPTIME, false, timerSettings_stopTime.getValue (), &timerSettings_stopTime );

	strcpy(m_weekdaysStr,"-------");
	CStringInput timerSettings_weekdays(LOCALE_TIMERLIST_WEEKDAYS, m_weekdaysStr, 7, LOCALE_TIMERLIST_WEEKDAYS_HINT_1, LOCALE_TIMERLIST_WEEKDAYS_HINT_2, "-X");
	CMenuForwarder *m4 = new CMenuForwarder(LOCALE_TIMERLIST_WEEKDAYS, false,  m_weekdaysStr, &timerSettings_weekdays);
	CTimerListRepeatNotifier notifier((int *)&timerNew.eventRepeat,m4);
	CMenuOptionChooser* m3 = new CMenuOptionChooser(LOCALE_TIMERLIST_REPEAT, (int *)&timerNew.eventRepeat, TIMERLIST_REPEAT_OPTIONS, TIMERLIST_REPEAT_OPTION_COUNT, true, &notifier);

	CZapitClient zapit;
	CZapitClient::BouquetList bouquetlist;
	zapit.getBouquets(bouquetlist, false, true); // UTF-8
	CZapitClient::BouquetList::iterator bouquet = bouquetlist.begin();
	CMenuWidget mctv(LOCALE_TIMERLIST_BOUQUETSELECT, NEUTRINO_ICON_SETTINGS);
	CMenuWidget mcradio(LOCALE_TIMERLIST_BOUQUETSELECT, NEUTRINO_ICON_SETTINGS);
	for(; bouquet != bouquetlist.end();bouquet++)
	{
		CMenuWidget* mwtv = new CMenuWidget(LOCALE_TIMERLIST_CHANNELSELECT, NEUTRINO_ICON_SETTINGS);
		toDelete.push_back(mwtv);
		CMenuWidget* mwradio = new CMenuWidget(LOCALE_TIMERLIST_CHANNELSELECT, NEUTRINO_ICON_SETTINGS);
		toDelete.push_back(mwradio);
		CZapitClient::BouquetChannelList subchannellist;
		zapit.getBouquetChannels(bouquet->bouquet_nr,subchannellist,CZapitClient::MODE_TV, true); // UTF-8
		CZapitClient::BouquetChannelList::iterator channel = subchannellist.begin();
		for(; channel != subchannellist.end();channel++)
		{
			char cChannelId[3+16+1+1];
			sprintf(cChannelId,
				"SC:"
				PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
				",",
				channel->channel_id);
			mwtv->addItem(new CMenuForwarderNonLocalized(channel->name, true, NULL, this, (std::string(cChannelId) + channel->name).c_str()));
		}
		if (!subchannellist.empty())
			mctv.addItem(new CMenuForwarderNonLocalized(bouquet->name, true, NULL, mwtv));
		subchannellist.clear();
		zapit.getBouquetChannels(bouquet->bouquet_nr,subchannellist,CZapitClient::MODE_RADIO, true); // UTF-8
		channel = subchannellist.begin();
		for(; channel != subchannellist.end();channel++)
		{
			char cChannelId[3+16+1+1];
			sprintf(cChannelId,
				"SC:"
				PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
				",",
				channel->channel_id);
			mwradio->addItem(new CMenuForwarderNonLocalized(channel->name, true, NULL, this, (std::string(cChannelId) + channel->name).c_str()));
		}
		if (!subchannellist.empty())
			mcradio.addItem(new CMenuForwarderNonLocalized(bouquet->name, true, NULL, mwradio));
	}
	CMenuWidget mm(LOCALE_TIMERLIST_MODESELECT, NEUTRINO_ICON_SETTINGS);
	mm.addItem(new CMenuForwarder(LOCALE_TIMERLIST_MODETV, true, NULL, &mctv));
	mm.addItem(new CMenuForwarder(LOCALE_TIMERLIST_MODERADIO, true, NULL, &mcradio));
	strcpy(timerNew_channel_name,"---");
	CMenuForwarder* m5 = new CMenuForwarder(LOCALE_TIMERLIST_CHANNEL, false, timerNew_channel_name, &mm); 

	CMenuOptionChooser* m6 = new CMenuOptionChooser(LOCALE_TIMERLIST_STANDBY, &timerNew_standby_on, TIMERLIST_STANDBY_OPTIONS, TIMERLIST_STANDBY_OPTION_COUNT, false); 

	CStringInputSMS timerSettings_msg(LOCALE_TIMERLIST_MESSAGE, timerNew.message, 30, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "abcdefghijklmnopqrstuvwxyz0123456789-.,:!?/ ");
	CMenuForwarder *m7 = new CMenuForwarder(LOCALE_TIMERLIST_MESSAGE, false, timerNew.message, &timerSettings_msg );

	strcpy(timerNew.pluginName,"---");
	CPluginChooser plugin_chooser(LOCALE_TIMERLIST_PLUGIN, CPlugins::P_TYPE_SCRIPT | CPlugins::P_TYPE_TOOL, timerNew.pluginName);
	CMenuForwarder *m8 = new CMenuForwarder(LOCALE_TIMERLIST_PLUGIN, false, timerNew.pluginName, &plugin_chooser);

	
	CTimerListNewNotifier notifier2((int *)&timerNew.eventType,
									&timerNew.stopTime,m2,m5,m6,m7,m8,
									timerSettings_stopTime.getValue ());
	CMenuOptionChooser* m0 = new CMenuOptionChooser(LOCALE_TIMERLIST_TYPE, (int *)&timerNew.eventType, TIMERLIST_TYPE_OPTIONS, TIMERLIST_TYPE_OPTION_COUNT, true, &notifier2); 

	timerSettings.addItem( m0);
	timerSettings.addItem( m1);
	timerSettings.addItem( m2);
	timerSettings.addItem( m3);
	timerSettings.addItem( m4);
	timerSettings.addItem( m5);
	timerSettings.addItem( m6);
	timerSettings.addItem( m7);
	timerSettings.addItem( m8);
	timerSettings.addItem(new CMenuForwarder(LOCALE_TIMERLIST_SAVE, true, NULL, this, "newtimer"));
	strcpy(timerSettings_stopTime.getValue (), "                ");
	
	int ret=timerSettings.exec(this,"");
	// delete dynamic created objects
	for(unsigned int count=0;count<toDelete.size();count++)
	{
		delete toDelete[count];
	}
	toDelete.clear();

	return ret;
}
