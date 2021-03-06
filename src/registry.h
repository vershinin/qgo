/***************************************************************************
 *   Copyright (C) 2009 by The qGo Project                                 *
 *                                                                         *
 *   This file is part of qGo.   					   *
 *                                                                         *
 *   qGo is free software: you can redistribute it and/or modify           *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 *   or write to the Free Software Foundation, Inc.,                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


#ifndef REGISTRY_H
#define REGISTRY_H
#include <map>

/* This is to be subclassed to provide specific functionality.
 * Using getEntry with a passed object means that the registry
 * will create a duplicate of the passed object and store and
 * return that duplicate.  This is also used for updating objects
 * in the registry.
 * But this means that onErase must be implemented by the subclass
 * to delete the allocated objects.
 * getEntry on the other hand assumes that getNewEntry is implemented
 * and returns the necessary object and that the object is also
 * deleted elsewhere, although onErase could be implemented here
 * as well.
 * Also, I later added setEntry and getIfEntry and they seem to
 * disrupt things a bit.  Like I'm already altering it for things
 * I didn't anticipate and so the design is bad. */

template <typename key, typename object>
class Registry
{
	public:
		virtual ~Registry();
		object * getEntry(key, object *);
		object * getEntry(key);
		object * getIfEntry(key);
		void setEntry(key, object *);
		void deleteEntry(key);
		int getSize(void) { return registry.size(); };
	protected:
		virtual object * getNewEntry(key) { return 0; };
		virtual void initEntry(object *) {};
		virtual void onErase(object * o) { delete o; };
		std::map <key, object *> * getStorage(void) { return &registry; };
	private:
		std::map <key, object *> registry;
};

template <typename key, typename object>
Registry<key,object>::~Registry()
{
	typename std::map<key, object *>::iterator i;
	for(i = registry.begin(); i != registry.end(); i++)
		 onErase(i->second);
	//registry.clear();
}

template <typename key, typename object>
object * Registry<key,object>::getEntry(key k, object * o)
{
	object * _o;
	typename std::map<key, object *>::iterator i;

	i = registry.find(k);
	if(i == registry.end())
	{
		_o = new object(*o);
		registry[k] = _o;
		initEntry(_o);
	}
	else
	{
		_o = i->second;
		*_o = *o;
		initEntry(_o);		//maybe we should have an extra updateEntry function too
	}
	return _o;
}

template <typename key, typename object>
object * Registry<key,object>::getEntry(key k)
{
	object * o = 0;
	typename std::map <key, object *>::iterator i;

	i = registry.find(k);
	if(i == registry.end())
	{
		o = getNewEntry(k);
		if(o)			//safe but ugly
		{
			registry[k] = o;
			initEntry(o);
		}
	}
	else
		o = i->second;
	return o;
}

template <typename key, typename object>
object * Registry<key,object>::getIfEntry(key k)
{
	typename std::map <key, object *>::iterator i;

	i = registry.find(k);
	if(i == registry.end())
		return 0;
	else
		return i->second;
}

template <typename key, typename object>
void Registry<key,object>::setEntry(key k, object * o)
{
	typename std::map<key, object *>::iterator i;

	i = registry.find(k);
	if(i == registry.end())
	{
		registry[k] = o;
	}
	else
	{
		onErase(i->second);
		i->second = o;
	}
}

template <typename key, typename object>
void Registry<key,object>::deleteEntry(key k)
{
	typename std::map <key, object *>::iterator i;

	i = registry.find(k);
	if(i == registry.end()) {}
	else
	{
		object * o = i->second;
		registry.erase(i);
		onErase(o);
	}

}
#endif //REGISTRY_H
