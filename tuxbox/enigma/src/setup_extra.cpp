/*
 * setup_extra.cpp
 *
 * Copyright (C) 2003 Andreas Monzner <ghostrider@tuxbox.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * $Id: setup_extra.cpp,v 1.21 2005/01/20 21:04:03 ghostrider Exp $
 */
#include <enigma.h>
#include <setup_extra.h>
#include <setupengrab.h>
#include <setupnetwork.h>
#include <software_update.h>
#include <setup_rc.h>
#include <lib/dvb/decoder.h>
#include <lib/gui/emessage.h>
#include <lib/system/info.h>

eExpertSetup::eExpertSetup()
	:eSetupWindow(_("Expert Setup"), 10, 400)
{
	cmove(ePoint(170, 115));

	int lockWebIf=1;
	if ( eConfig::getInstance()->getKey("/ezap/webif/lockWebIf", lockWebIf) )
		eConfig::getInstance()->setKey("/ezap/webif/lockWebIf", lockWebIf);

	int showSatPos=1;
	if ( eConfig::getInstance()->getKey("/extras/showSatPos", showSatPos) )
		eConfig::getInstance()->setKey("/extras/showSatPos", showSatPos);

	int entry=0;
#ifndef DISABLE_NETWORK
	if (eSystemInfo::getInstance()->hasNetwork())
	{
		CONNECT((new eListBoxEntryMenu(&list, _("Communication Setup"), eString().sprintf("(%d) %s", ++entry, _("open communication setup")) ))->selected, eExpertSetup::communication_setup);
		CONNECT((new eListBoxEntryMenu(&list, _("Ngrab Streaming Setup"), eString().sprintf("(%d) %s", ++entry, _("open ngrab server setup")) ))->selected, eExpertSetup::ngrab_setup);
		if ( eSystemInfo::getInstance()->getHwType() != eSystemInfo::DM7020 )  // no update for 7020 yet
			CONNECT((new eListBoxEntryMenu(&list, _("Software Update"), eString().sprintf("(%d) %s", ++entry, _("open software update")) ))->selected, eExpertSetup::software_update);
	}
#endif
	CONNECT((new eListBoxEntryMenu(&list, _("Remote Control"), eString().sprintf("(%d) %s", ++entry, _("open remote control setup")) ))->selected, eExpertSetup::rc_setup);
	if ( eSystemInfo::getInstance()->getHwType() >= eSystemInfo::DM7000 )
		CONNECT((new eListBoxEntryMenu(&list, _("Factory reset"), eString().sprintf("(%d) %s", ++entry, _("all settings will set to factory defaults")) ))->selected, eExpertSetup::factory_reset);
	new eListBoxEntrySeparator( (eListBox<eListBoxEntry>*)&list, eSkin::getActive()->queryImage("listbox.separator"), 0, true );
#ifndef DISABLE_FILE
	list.setFlags(list.getFlags()|eListBoxBase::flagNoPageMovement);
	record_split_size = new eListBoxEntryMulti( (eListBox<eListBoxEntryMulti>*)&list, _("record split size (left, right)"));
	record_split_size->add("         650MB        >", 650*1024);
	record_split_size->add("<        700MB        >", 700*1024);
	record_split_size->add("<        800MB        >", 800*1024);
	record_split_size->add("<         1GB         >", 1024*1024);
	record_split_size->add("<        1,5GB        >", 1536*1024);
	record_split_size->add("<         2GB         >", 2*1024*1024);
	record_split_size->add("<         4GB         >", 4*1024*1024);
	record_split_size->add("<         8GB         >", 8*1024*1024);
	record_split_size->add("<        16GB         ", 16*1024*1024);
	int splitsize=0;
	if (eConfig::getInstance()->getKey("/extras/record_splitsize", splitsize))
		splitsize=1024*1024; // 1G
	record_split_size->setCurrent(splitsize);
#endif
	CONNECT((new eListBoxEntryCheck((eListBox<eListBoxEntry>*)&list,_("Serviceselector help buttons"),"/ezap/serviceselector/showButtons",_("show colored help buttons in service selector")))->selected, eExpertSetup::colorbuttonsChanged );
	new eListBoxEntryCheck( (eListBox<eListBoxEntry>*)&list, _("Show Sat position"), "/extras/showSatPos", _("show sat position in the infobar"));
	if ( eSystemInfo::getInstance()->getHwType() >= eSystemInfo::DM7000 )
		CONNECT((new eListBoxEntryCheck((eListBox<eListBoxEntry>*)&list,_("Enable fast zapping"),"/elitedvb/extra/fastzapping",_("enables faster zapping.. but with visible sync")))->selected, eExpertSetup::fastZappingChanged );
	new eListBoxEntryCheck( (eListBox<eListBoxEntry>*)&list, _("Skip confirmations"), "/elitedvb/extra/profimode", _("enable/disable confirmations"));
	new eListBoxEntryCheck( (eListBox<eListBoxEntry>*)&list, _("Hide error windows"), "/elitedvb/extra/hideerror", _("don't show zap error messages"));
	new eListBoxEntryCheck( (eListBox<eListBoxEntry>*)&list, _("Auto show Infobar"), "/ezap/osd/showOSDOnEITUpdate", _("always show infobar when new event info is avail"));
	new eListBoxEntryCheck( (eListBox<eListBoxEntry>*)&list, _("Show remaining Time"), "/ezap/osd/showCurrentRemaining", _("show event remaining time in the infobar"));
	CONNECT((new eListBoxEntryCheck( (eListBox<eListBoxEntry>*)&list, _("Use http authentification"), "/ezap/webif/lockWebIf", _("enables the http (user/password) authentification")))->selected, eExpertSetup::reinitializeHTTPServer );
	CONNECT((new eListBoxEntryCheck( (eListBox<eListBoxEntry>*)&list, _("Don't open serial port"), "/ezap/extra/disableSerialOutput", _("don't write debug messages to /dev/tts/0")))->selected, eExpertSetup::reinitializeHTTPServer );
	new eListBoxEntryCheck( (eListBox<eListBoxEntry>*)&list, _("Auto bouquet change"), "/elitedvb/extra/autobouquetchange", _("change into next bouquet when end of current bouquet is reached"));
	setHelpID(92);
	CONNECT( list.selchanged, eExpertSetup::selChanged );
}

void eExpertSetup::selChanged(eListBoxEntryMenu* e)
{
	if ( e == (eListBoxEntryMenu*)record_split_size )
		eConfig::getInstance()->setKey("/extras/record_splitsize", (int)e->getKey());
}

void eExpertSetup::colorbuttonsChanged(bool b)
{
	eServiceSelector *sel = eZap::getInstance()->getServiceSelector();
	sel->setStyle( sel->getStyle(), true );
}

void eExpertSetup::reinitializeHTTPServer(bool)
{
	eZap::getInstance()->reconfigureHTTPServer();
}

void eExpertSetup::fastZappingChanged(bool b)
{
	Decoder::setFastZap(b);
}

#ifndef DISABLE_NETWORK
void eExpertSetup::communication_setup()
{
	hide();
	eZapNetworkSetup setup;
#ifndef DISABLE_LCD
	setup.setLCD(LCDTitle, LCDElement);
#endif
	setup.show();
	setup.exec();
	setup.hide();
	show();
}

void eExpertSetup::ngrab_setup()
{
	hide();
	ENgrabSetup setup;
#ifndef DISABLE_LCD
	setup.setLCD(LCDTitle, LCDElement);
#endif
	setup.show();
	setup.exec();
	setup.hide();
	show();
}

void eExpertSetup::software_update()
{
	hide();
	eSoftwareUpdate setup;
#ifndef DISABLE_LCD
	setup.setLCD(LCDTitle, LCDElement);
#endif
	setup.show();
	setup.exec();
	setup.hide();
	show();
}

#endif

void eExpertSetup::rc_setup()
{
	hide();
	eZapRCSetup setup;
#ifndef DISABLE_LCD
	setup.setLCD(LCDTitle, LCDElement);
#endif
	setup.show();
	setup.exec();
	setup.hide();
	show();
}

//implemented in upgrade.cpp
extern bool erase(char mtd[30], const char *titleText);

void eExpertSetup::factory_reset()
{
	hide();
	eMessageBox mb(
		_("When you do a factory reset, you will lost ALL your configuration data\n"
			"(including bouquets, services, satellite data ...)\n"
			"After finishing the factory reset, your receiver restarts automatically!\n\n"
			"Really do factory reset?"),
		_("Factory reset"),
		eMessageBox::btYes|eMessageBox::btNo|eMessageBox::iconQuestion,
		eMessageBox::btNo );
	mb.show();
	int ret = mb.exec();
	mb.hide();
	if ( ret == eMessageBox::btYes ) 
	{
		switch( eSystemInfo::getInstance()->getHwType() )
		{
			case eSystemInfo::DM7020:
				system("rm -R /etc/enigma && killall -9 enigma");
				break;
			case eSystemInfo::DM7000:
			case eSystemInfo::DM500:
			case eSystemInfo::DM5620:
			case eSystemInfo::DM5600:
			case eSystemInfo::TR_DVB272S:
				erase("/dev/mtd/1", _("Factory reset..."));
				system("reboot");
				break;
			default: 
				eDebug("factory reset not implemented for this hardware!!\n");
		}
	}
	show();
}
