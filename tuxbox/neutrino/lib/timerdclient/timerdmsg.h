/*
 * $Header: /cvs/tuxbox/apps/tuxbox/neutrino/lib/timerdclient/timerdmsg.h,v 1.8 2004/12/18 17:46:25 chakazulu Exp $
 *
 * types used for clientlib <-> timerd communication - d-box2 linux project
 *
 * (C) 2002 by thegoodguy <thegoodguy@berlios.de>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __timerdmsg_h__
#define __timerdmsg_h__


#include <connection/basicmessage.h>

#include "timerdtypes.h" // REMINDER_MESSAGE_MAXLEN


#define TIMERD_UDS_NAME "/tmp/timerd.sock"


class CTimerdMsg : public CBasicMessage
{
	
 public:

	static const CBasicMessage::t_version ACTVERSION = 2;

	enum commands
		{
			CMD_ADDTIMER = 1,
			CMD_REMOVETIMER,
			CMD_GETTIMER,
			CMD_GETTIMERLIST,
			CMD_MODIFYTIMER,
			CMD_GETSLEEPTIMER,
			CMD_RESCHEDULETIMER,

			CMD_REGISTEREVENT,
			CMD_UNREGISTEREVENT,
			CMD_TIMERDAVAILABLE,
			CMD_SHUTDOWN,
			CMD_SETAPID,
			CMD_GETRECSAFETY,
			CMD_SETRECSAFETY,
			CMD_STOPTIMER
		};


	struct commandAddTimer
	{
		CTimerd::CTimerEventTypes  eventType;
		CTimerd::CTimerEventRepeat eventRepeat;
		time_t                     alarmTime;
		time_t                     announceTime;
		time_t                     stopTime;			
	};

	struct commandGetTimer
	{
		int   eventID;
	};

	struct commandModifyTimer
	{
		int                        eventID;
		time_t                     announceTime;
		time_t                     alarmTime;
		time_t                     stopTime;
		CTimerd::CTimerEventRepeat eventRepeat;
	};

	struct commandRemind
	{
		char message[REMINDER_MESSAGE_MAXLEN];
	};

	struct commandExecPlugin
	{
		char name[EXEC_PLUGIN_MESSAGE_MAXLEN];
	};

	struct commandSetAPid
	{
		int   eventID;
		char  apids[TIMERD_APIDS_MAXLEN];
	};

	struct commandRemoveTimer
	{
		int   eventID;
	};

	struct commandSetStandby
	{
		bool standby_on;
	};

	struct commandRecordingSafety
	{
		int pre;
		int post;
	};



	struct generalInteger
	{
		int number;
	};


	struct responseAddTimer
	{
		int   eventID;
	};

	struct responseAvailable
	{
		bool available;
	};
		
	struct responseGetSleeptimer
	{
		int   eventID;
	};

	struct responseStatus
	{
		bool status;
	};

};

#endif /* __timerdmsg_h__ */
