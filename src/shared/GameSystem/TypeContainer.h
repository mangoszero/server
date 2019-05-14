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

#ifndef MANGOS_TYPECONTAINER_H
#define MANGOS_TYPECONTAINER_H

/*
 * Here, you'll find a series of containers that allow you to hold multiple
 * types of object at the same time.
 *
 * Refactored for C++11 by H0zen
 */

#include <cstddef>
#include <tuple>
#include <unordered_map>
#include "GameSystem/GridRefManager.h"


// various metaprogramming primitives
namespace Meta
{
  // Gets the index of specified type T in a std::tuple
  template <class T, class Tuple> struct IndexOf;

  template <class T, class... Types> struct IndexOf<T, std::tuple<T, Types...>>
  {
    static const std::size_t value = 0;
  };

  template <class T, class U, class... Types> struct IndexOf<T, std::tuple<U, Types...>>
  {
    static const std::size_t value = 1 + IndexOf<T, std::tuple<Types...>>::value;
  };
  //----------------------------------------------------------------------------------------

  // apply a transformation on each element of a tuple
  template<template<class...> class F, class L> struct Transform_Impl;

  template<template<class...> class F, template<class...> class L, class... T>
  struct Transform_Impl<F, L<T...>>
  {
    using type = L<F<T>...>;
  };

  template<template<class...> class F, class L>
  using Transform = typename Transform_Impl<F, L>::type;
  //----------------------------------------------------------------------------------------

  // convert a tuple A into another tuple B
  template<class A, template<class...> class B> struct Rename_Impl;

  template<template<class...> class A, class... T, template<class...> class B>
  struct Rename_Impl<A<T...>, B>
  {
    using type = B<T...>;
  };

  template<class A, template<class...> class B>
  using Rename = typename Rename_Impl<A, B>::type;
  //----------------------------------------------------------------------------------------

  //tuple iteration
  template<size_t index, typename F, typename... Ts>
  struct iterate_tuple {
     void operator() (std::tuple<Ts...>&& t, F&& callback) {
         iterate_tuple<index - 1, F, Ts...>{}(std::forward<std::tuple<Ts...>>(t), std::forward<F>(callback));
         callback.Visit(std::get<index>(t));
     }
  };

  template<typename F, typename... Ts>
  struct iterate_tuple<0, F, Ts...> {
     void operator() (std::tuple<Ts...>&& t, F&& callback) {
         callback.Visit(std::get<0>(t));
     }
  };

  template<typename F, typename... Ts>
  void for_each(std::tuple<Ts...>&& t, F&& callback) {
     iterate_tuple<std::tuple_size<std::tuple<Ts...>>::value - 1, F, Ts...> it;
     it(std::forward<std::tuple<Ts...>>(t), std::forward<F>(callback));
  }

} //Meta namespace end


template<typename KEY_TYPE, typename TYPE_LIST>
class TypeUnorderedMapContainer
{
    using Tuple = Meta::Rename<TYPE_LIST,std::tuple>;
    template <typename T> using add_pointer = T*;
    template <typename T> using add_wrap = std::unordered_map<KEY_TYPE, T>;

    using Container = Meta::Transform<add_wrap , Meta::Transform<add_pointer, Tuple>>;

    public:
        template <typename T>
        bool insert(KEY_TYPE handle, T* object)
        {
            auto&& _element = std::get<Meta::IndexOf<T,Tuple>::value>(i_container);
            if (_element.end() == _element.find(handle))
            {
                _element[handle] = object;
                return true;
            }
            else
            {
                assert(_element[handle] == object && "Object with certain key already in but objects are different!");
                return false;
            }
        }
        template <typename T>
        bool erase(KEY_TYPE handle, T*)
        {
            std::get<Meta::IndexOf<T,Tuple>::value>(i_container).erase(handle);
            return true;
        }

        template <typename T>
        T* find (KEY_TYPE handle, T*)
        {
            auto&& _element = std::get<Meta::IndexOf<T,Tuple>::value>(i_container);
            auto&& iter = _element.find(handle);
            if (iter == _element.end())
              { return nullptr; }
            else
              { return iter->second; }
        }

    private:
      Container i_container;
};

//TypeMapContainer

template<typename TYPE_LIST>
class TypeMapContainer
{
    using Tuple = Meta::Rename<TYPE_LIST,std::tuple>;
    template <typename T> using add_wrap = GridRefManager<T>;

    using Container = Meta::Transform<add_wrap, Tuple>;

    public:
        template <typename T>
        size_t count(T*) const 
        {
            return std::get<Meta::IndexOf<T,Tuple>::value>(i_container).getSize();
        }

        template <typename T>
        T* insert(T* obj)
        {
            obj->GetGridRef().link(&std::get<Meta::IndexOf<T,Tuple>::value>(i_container), obj);
            return obj;
        }

        template <typename T>
        T* remove(T* obj)
        {
            obj->GetGridRef().unlink();
            return obj;
        }

        template <typename Visitor>
        void accept(Visitor&& v)
        {
            Meta::for_each(std::forward<Container>(i_container), std::forward<Visitor>(v));
        }

    private:
      Container i_container;
};

#endif
