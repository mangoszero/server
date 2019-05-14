/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef MANGOS_H_REFERENCE
#define MANGOS_H_REFERENCE

#include "Utilities/LinkedList.h"

//=====================================================

template<class TO, class FROM>
/**
 * @brief
 *
 */
class Reference : public LinkedListElement
{
    private:

        TO* iRefTo; /**< TODO */
        FROM* iRefFrom; /**< TODO */

    protected:

        /**
         * @brief Tell our refTo (target) object that we have a link
         *
         */
        virtual void targetObjectBuildLink() = 0;

        /**
         * @brief Tell our refTo (taget) object, that the link is cut
         *
         */
        virtual void targetObjectDestroyLink() = 0;

        /**
         * @brief Tell our refFrom (source) object, that the link is cut (Target destroyed)
         *
         */
        virtual void sourceObjectDestroyLink() = 0;

    public:

        /**
         * @brief
         *
         */
        Reference()
            : iRefTo(NULL), iRefFrom(NULL)
        {
        }

        /**
         * @brief
         *
         */
        virtual ~Reference() {}

        /**
         * @brief Create new link
         *
         * @param toObj
         * @param fromObj
         */
        void link(TO* toObj, FROM* fromObj)
        {
            assert(fromObj);                                // fromObj MUST not be NULL
            if (isValid())
                { unlink(); }

            if (toObj != NULL)
            {
                iRefTo = toObj;
                iRefFrom = fromObj;
                targetObjectBuildLink();
            }
        }

        /**
         * @brief We don't need the reference anymore.
         *
         * Call comes from the refFrom object. Tell our refTo object, that the
         * link is cut.
         *
         */
        void unlink()
        {
            targetObjectDestroyLink();
            delink();
            iRefTo = NULL;
            iRefFrom = NULL;
        }

        /**
         * @brief Link is invalid due to destruction of referenced target object.
         *
         * Call comes from the refTo object. Tell our refFrom object, that the
         * link is cut.
         *
         */
        void invalidate()                                   // the iRefFrom MUST remain!!
        {
            sourceObjectDestroyLink();
            delink();
            iRefTo = NULL;
        }

        /**
         * @brief
         *
         * @return bool
         */
        bool isValid() const                                // Only check the iRefTo
        {
            return iRefTo != NULL;
        }

        /**
         * @brief
         *
         * @return Reference<TO, FROM>
         */
        Reference<TO, FROM>*       next()       { return((Reference<TO, FROM>*) LinkedListElement::next()); }
        /**
         * @brief
         *
         * @return const Reference<TO, FROM>
         */
        Reference<TO, FROM> const* next() const { return((Reference<TO, FROM> const*) LinkedListElement::next()); }
        /**
         * @brief
         *
         * @return Reference<TO, FROM>
         */
        Reference<TO, FROM>*       prev()       { return((Reference<TO, FROM>*) LinkedListElement::prev()); }
        /**
         * @brief
         *
         * @return const Reference<TO, FROM>
         */
        Reference<TO, FROM> const* prev() const { return((Reference<TO, FROM> const*) LinkedListElement::prev()); }

        /**
         * @brief
         *
         * @return Reference<TO, FROM>
         */
        Reference<TO, FROM>*       nocheck_next()       { return((Reference<TO, FROM>*) LinkedListElement::nocheck_next()); }
        /**
         * @brief
         *
         * @return const Reference<TO, FROM>
         */
        Reference<TO, FROM> const* nocheck_next() const { return((Reference<TO, FROM> const*) LinkedListElement::nocheck_next()); }
        /**
         * @brief
         *
         * @return Reference<TO, FROM>
         */
        Reference<TO, FROM>*       nocheck_prev()       { return((Reference<TO, FROM>*) LinkedListElement::nocheck_prev()); }
        /**
         * @brief
         *
         * @return const Reference<TO, FROM>
         */
        Reference<TO, FROM> const* nocheck_prev() const { return((Reference<TO, FROM> const*) LinkedListElement::nocheck_prev()); }

        /**
         * @brief
         *
         * @return TO *operator ->
         */
        TO* operator->() const { return iRefTo; }
        /**
         * @brief
         *
         * @return TO
         */
        TO* getTarget() const { return iRefTo; }

        /**
         * @brief
         *
         * @return FROM
         */
        FROM* getSource() const { return iRefFrom; }
};

//=====================================================

#endif
