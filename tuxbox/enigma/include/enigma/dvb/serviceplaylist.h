#ifndef __core_dvb_serviceplaylist_h
#define __core_dvb_serviceplaylist_h

#include <core/dvb/service.h>
#include <list>

class ePlaylist: public eService
{
public:
	std::list<eServiceReference> list;
	std::list<eServiceReference>::iterator current;

	int load(const char *filename);
	int save(const char *filename);

	ePlaylist();
};

class eServicePlaylistHandler: public eServiceHandler
{
	static eServicePlaylistHandler *instance; 
	void addFile(void *node, const eString &filename);

	std::multimap<eServiceReference,eServiceReference> playlists;

public:
	enum { ID = 0x1001 } ;
	static eServicePlaylistHandler *getInstance() { return instance; }

	eService *createService(const eServiceReference &node);
	
	eServicePlaylistHandler();
	~eServicePlaylistHandler();

		// service list functions
	void enterDirectory(const eServiceReference &dir, Signal1<void,const eServiceReference&> &callback);
	void leaveDirectory(const eServiceReference &dir);

	eService *addRef(const eServiceReference &service);
	void removeRef(const eServiceReference &service);

		// playlist functions
	eServiceReference newPlaylist(const eServiceReference &parent=eServiceReference(), const eServiceReference &serviceref=eServiceReference());
	void removePlaylist(const eServiceReference &service);
};

#endif
