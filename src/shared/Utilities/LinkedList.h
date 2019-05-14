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

#ifndef _LINKEDLIST
#define _LINKEDLIST

#include "Common.h"

//============================================
class LinkedListHead;

/**
 * @brief
 *
 */
class LinkedListElement
{
    private:

        friend class LinkedListHead;

        LinkedListElement* iNext; /**< TODO */
        LinkedListElement* iPrev; /**< TODO */

    public:

        /**
         * @brief
         *
         */
        LinkedListElement()  { iNext = NULL; iPrev = NULL; }
        /**
         * @brief
         *
         */
        ~LinkedListElement() { delink(); }

        /**
         * @brief
         *
         * @return bool
         */
        bool hasNext() const  { return (iNext->iNext != NULL); }
        /**
         * @brief
         *
         * @return bool
         */
        bool hasPrev() const  { return (iPrev->iPrev != NULL); }
        /**
         * @brief
         *
         * @return bool
         */
        bool isInList() const { return (iNext != NULL && iPrev != NULL); }

        /**
         * @brief
         *
         * @return LinkedListElement
         */
        LinkedListElement*       next()       { return hasNext() ? iNext : NULL; }
        /**
         * @brief
         *
         * @return const LinkedListElement
         */
        LinkedListElement const* next() const { return hasNext() ? iNext : NULL; }
        /**
         * @brief
         *
         * @return LinkedListElement
         */
        LinkedListElement*       prev()       { return hasPrev() ? iPrev : NULL; }
        /**
         * @brief
         *
         * @return const LinkedListElement
         */
        LinkedListElement const* prev() const { return hasPrev() ? iPrev : NULL; }

        /**
         * @brief
         *
         * @return LinkedListElement
         */
        LinkedListElement*       nocheck_next()       { return iNext; }
        /**
         * @brief
         *
         * @return const LinkedListElement
         */
        LinkedListElement const* nocheck_next() const { return iNext; }
        /**
         * @brief
         *
         * @return LinkedListElement
         */
        LinkedListElement*       nocheck_prev()       { return iPrev; }
        /**
         * @brief
         *
         * @return const LinkedListElement
         */
        LinkedListElement const* nocheck_prev() const { return iPrev; }

        /**
         * @brief
         *
         */
        void delink()
        {
            if (isInList())
            {
                iNext->iPrev = iPrev;
                iPrev->iNext = iNext;
                iNext = NULL;
                iPrev = NULL;
            }
        }

        /**
         * @brief
         *
         * @param pElem
         */
        void insertBefore(LinkedListElement* pElem)
        {
            pElem->iNext = this;
            pElem->iPrev = iPrev;
            iPrev->iNext = pElem;
            iPrev = pElem;
        }

        /**
         * @brief
         *
         * @param pElem
         */
        void insertAfter(LinkedListElement* pElem)
        {
            pElem->iPrev = this;
            pElem->iNext = iNext;
            iNext->iPrev = pElem;
            iNext = pElem;
        }
};

//============================================

/**
 * @brief
 *
 */
class LinkedListHead
{
    private:

        LinkedListElement iFirst; /**< TODO */
        LinkedListElement iLast; /**< TODO */
        uint32 iSize; /**< TODO */

    public:

        /**
         * @brief
         *
         */
        LinkedListHead()
        {
            // create empty list

            iFirst.iNext = &iLast;
            iLast.iPrev = &iFirst;
            iSize = 0;
        }

        /**
         * @brief
         *
         * @return bool
         */
        bool isEmpty() const { return (!iFirst.iNext->isInList()); }

        /**
         * @brief
         *
         * @return LinkedListElement
         */
        LinkedListElement*       getFirst()       { return (isEmpty() ? NULL : iFirst.iNext); }
        /**
         * @brief
         *
         * @return const LinkedListElement
         */
        LinkedListElement const* getFirst() const { return (isEmpty() ? NULL : iFirst.iNext); }

        /**
         * @brief
         *
         * @return LinkedListElement
         */
        LinkedListElement*       getLast()        { return (isEmpty() ? NULL : iLast.iPrev); }
        /**
         * @brief
         *
         * @return const LinkedListElement
         */
        LinkedListElement const* getLast() const  { return (isEmpty() ? NULL : iLast.iPrev); }

        /**
         * @brief
         *
         * @param pElem
         */
        void insertFirst(LinkedListElement* pElem)
        {
            iFirst.insertAfter(pElem);
        }

        /**
         * @brief
         *
         * @param pElem
         */
        void insertLast(LinkedListElement* pElem)
        {
            iLast.insertBefore(pElem);
        }

        /**
         * @brief
         *
         * @return uint32
         */
        uint32 getSize() const
        {
            if (!iSize)
            {
                uint32 result = 0;
                LinkedListElement const* e = getFirst();

                while (e)
                {
                    ++result;
                    e = e->next();
                }

                return result;
            }
            else
                { return iSize; }
        }

        /**
         * @brief
         *
         */
        void incSize() { ++iSize; }
        /**
         * @brief
         *
         */
        void decSize() { --iSize; }

        template<class _Ty>
        /**
         * @brief
         *
         */
        class Iterator
        {
            public:

                /**
                 * @brief
                 *
                 */
                typedef std::bidirectional_iterator_tag iterator_category;
                /**
                 * @brief
                 *
                 */
                typedef _Ty value_type;
                /**
                 * @brief
                 *
                 */
                typedef ptrdiff_t difference_type;
                /**
                 * @brief
                 *
                 */
                typedef ptrdiff_t distance_type;
                /**
                 * @brief
                 *
                 */
                typedef _Ty* pointer;
                /**
                 * @brief
                 *
                 */
                typedef _Ty const* const_pointer;
                /**
                 * @brief
                 *
                 */
                typedef _Ty& reference;
                /**
                 * @brief
                 *
                 */
                typedef _Ty const& const_reference;

                /**
                 * @brief
                 *
                 */
                Iterator()
                    : _Ptr(0)
                {
                    // construct with null node pointer
                }

                /**
                 * @brief
                 *
                 * @param _Pnode
                 */
                Iterator(pointer _Pnode)
                    : _Ptr(_Pnode)
                {
                    // construct with node pointer _Pnode
                }

                /**
                 * @brief
                 *
                 * @param _Right
                 * @return Iterator &operator
                 */
                Iterator& operator=(Iterator const& _Right)
                {
                    return (*this) = _Right._Ptr;
                }

                /**
                 * @brief
                 *
                 * @param _Right
                 * @return Iterator &operator
                 */
                Iterator& operator=(const_pointer const& _Right)
                {
                    _Ptr = (pointer)_Right;
                    return (*this);
                }

                /**
                 * @brief
                 *
                 * @return reference operator
                 */
                reference operator*()
                {
                    // return designated value
                    return *_Ptr;
                }

                /**
                 * @brief
                 *
                 * @return pointer operator ->
                 */
                pointer operator->()
                {
                    // return pointer to class object
                    return _Ptr;
                }

                /**
                 * @brief
                 *
                 * @return Iterator &operator
                 */
                Iterator& operator++()
                {
                    // preincrement
                    _Ptr = _Ptr->next();
                    return (*this);
                }

                /**
                 * @brief
                 *
                 * @param int
                 * @return Iterator operator
                 */
                Iterator operator++(int)
                {
                    // postincrement
                    iterator _Tmp = *this;
                    ++*this;
                    return (_Tmp);
                }

                /**
                 * @brief
                 *
                 * @return Iterator &operator
                 */
                Iterator& operator--()
                {
                    // predecrement
                    _Ptr = _Ptr->prev();
                    return (*this);
                }

                /**
                 * @brief
                 *
                 * @param int
                 * @return Iterator operator
                 */
                Iterator operator--(int)
                {
                    // postdecrement
                    iterator _Tmp = *this;
                    --*this;
                    return (_Tmp);
                }

                /**
                 * @brief
                 *
                 * @param _Right
                 * @return bool operator
                 */
                bool operator==(Iterator const& _Right) const
                {
                    // test for iterator equality
                    return (_Ptr == _Right._Ptr);
                }

                /**
                 * @brief
                 *
                 * @param _Right
                 * @return bool operator
                 */
                bool operator!=(Iterator const& _Right) const
                {
                    // test for iterator inequality
                    return (!(*this == _Right));
                }

                /**
                 * @brief
                 *
                 * @param _Right
                 * @return bool operator
                 */
                bool operator==(pointer const& _Right) const
                {
                    // test for pointer equality
                    return (_Ptr != _Right);
                }

                /**
                 * @brief
                 *
                 * @param _Right
                 * @return bool operator
                 */
                bool operator!=(pointer const& _Right) const
                {
                    // test for pointer equality
                    return (!(*this == _Right));
                }

                /**
                 * @brief
                 *
                 * @param _Right
                 * @return bool operator
                 */
                bool operator==(const_reference _Right) const
                {
                    // test for reference equality
                    return (_Ptr == &_Right);
                }

                /**
                 * @brief
                 *
                 * @param _Right
                 * @return bool operator
                 */
                bool operator!=(const_reference _Right) const
                {
                    // test for reference equality
                    return (_Ptr != &_Right);
                }

                /**
                 * @brief
                 *
                 * @return pointer
                 */
                pointer _Mynode()
                {
                    // return node pointer
                    return (_Ptr);
                }

            protected:

                pointer _Ptr;                               /**< pointer to node */
        };

        /**
         * @brief
         *
         */
        typedef Iterator<LinkedListElement> iterator;
};

//============================================

#endif
