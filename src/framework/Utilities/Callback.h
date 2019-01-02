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

#ifndef MANGOS_CALLBACK_H
#define MANGOS_CALLBACK_H

// defines to simplify multi param templates code and readablity
#define TYPENAMES_1 typename T1
#define TYPENAMES_2 TYPENAMES_1, typename T2
#define TYPENAMES_3 TYPENAMES_2, typename T3
#define TYPENAMES_4 TYPENAMES_3, typename T4
#define TYPENAMES_5 TYPENAMES_4, typename T5
#define TYPENAMES_6 TYPENAMES_5, typename T6
#define TYPENAMES_7 TYPENAMES_6, typename T7
#define TYPENAMES_8 TYPENAMES_7, typename T8
#define TYPENAMES_9 TYPENAMES_8, typename T9
#define TYPENAMES_10 TYPENAMES_9, typename T10

#define PARAMS_1 T1 param1
#define PARAMS_2 PARAMS_1, T2 param2
#define PARAMS_3 PARAMS_2, T3 param3
#define PARAMS_4 PARAMS_3, T4 param4
#define PARAMS_5 PARAMS_4, T5 param5
#define PARAMS_6 PARAMS_5, T6 param6
#define PARAMS_7 PARAMS_6, T7 param7
#define PARAMS_8 PARAMS_7, T8 param8
#define PARAMS_9 PARAMS_8, T9 param9
#define PARAMS_10 PARAMS_9, T10 param10

/**
 * @brief empty struct to use in templates instead of void type
 *
 */
struct null { null() {} };
/// ------------ BASE CLASSES ------------

namespace MaNGOS
{
    template < class Class, typename ParamType1 = void, typename ParamType2 = void, typename ParamType3 = void, typename ParamType4 = void >
    /**
     * @brief
     *
     */
    class _Callback
    {
        protected:

            /**
             * @brief
             *
             */
            typedef void (Class::*Method)(ParamType1, ParamType2, ParamType3, ParamType4);
            Class* m_object; /**< TODO */
            Method m_method; /**< TODO */
            ParamType1 m_param1; /**< TODO */
            ParamType2 m_param2; /**< TODO */
            ParamType3 m_param3; /**< TODO */
            ParamType4 m_param4; /**< TODO */
            /**
             * @brief
             *
             */
            void _Execute() { (m_object->*m_method)(m_param1, m_param2, m_param3, m_param4); }

        public:

            /**
             * @brief
             *
             * @param object
             * @param method
             * @param param1
             * @param param2
             * @param param3
             * @param param4
             */
            _Callback(Class* object, Method method, ParamType1 param1, ParamType2 param2, ParamType3 param3, ParamType4 param4)
                : m_object(object), m_method(method),
                  m_param1(param1), m_param2(param2), m_param3(param3), m_param4(param4)
            {
            }

            /**
             * @brief
             *
             * @param const_Callback<Class
             * @param ParamType1
             * @param ParamType2
             * @param ParamType3
             * @param cb
             */
            _Callback(_Callback<Class, ParamType1, ParamType2, ParamType3, ParamType4> const& cb)
                : m_object(cb.m_object), m_method(cb.m_method),
                  m_param1(cb.m_param1), m_param2(cb.m_param2), m_param3(cb.m_param3), m_param4(cb.m_param4)
            {
            }
    };

    template<class Class, typename ParamType1, typename ParamType2, typename ParamType3>
    /**
     * @brief
     *
     */
    class _Callback<Class, ParamType1, ParamType2, ParamType3>
    {
        protected:

            /**
             * @brief
             *
             */
            typedef void (Class::*Method)(ParamType1, ParamType2, ParamType3);
            Class* m_object; /**< TODO */
            Method m_method; /**< TODO */
            ParamType1 m_param1; /**< TODO */
            ParamType2 m_param2; /**< TODO */
            ParamType3 m_param3; /**< TODO */
            /**
             * @brief
             *
             */
            void _Execute() { (m_object->*m_method)(m_param1, m_param2, m_param3); }

        public:
            /**
             * @brief
             *
             * @param object
             * @param method
             * @param param1
             * @param param2
             * @param param3
             */
            _Callback(Class* object, Method method, ParamType1 param1, ParamType2 param2, ParamType3 param3)
                : m_object(object), m_method(method),
                  m_param1(param1), m_param2(param2), m_param3(param3)
            {
            }

            /**
             * @brief
             *
             * @param const_Callback<Class
             * @param ParamType1
             * @param ParamType2
             * @param cb
             */
            _Callback(_Callback<Class, ParamType1, ParamType2, ParamType3> const& cb)
                : m_object(cb.m_object), m_method(cb.m_method),
                  m_param1(cb.m_param1), m_param2(cb.m_param2), m_param3(cb.m_param3)
            {
            }
    };

    template<class Class, typename ParamType1, typename ParamType2>
    /**
     * @brief
     *
     */
    class _Callback<Class, ParamType1, ParamType2>
    {
        protected:

            /**
             * @brief
             *
             */
            typedef void (Class::*Method)(ParamType1, ParamType2);
            Class* m_object; /**< TODO */
            Method m_method; /**< TODO */
            ParamType1 m_param1; /**< TODO */
            ParamType2 m_param2; /**< TODO */
            /**
             * @brief
             *
             */
            void _Execute() { (m_object->*m_method)(m_param1, m_param2); }

        public:

            /**
             * @brief
             *
             * @param object
             * @param method
             * @param param1
             * @param param2
             */
            _Callback(Class* object, Method method, ParamType1 param1, ParamType2 param2)
                : m_object(object), m_method(method),
                  m_param1(param1), m_param2(param2)
            {
            }

            /**
             * @brief
             *
             * @param const_Callback<Class
             * @param ParamType1
             * @param cb
             */
            _Callback(_Callback<Class, ParamType1, ParamType2> const& cb)
                : m_object(cb.m_object), m_method(cb.m_method),
                  m_param1(cb.m_param1), m_param2(cb.m_param2)
            {
            }
    };

    template<class Class, typename ParamType1>
    /**
     * @brief
     *
     */
    class _Callback<Class, ParamType1>
    {
        protected:

            /**
             * @brief
             *
             */
            typedef void (Class::*Method)(ParamType1);
            Class* m_object; /**< TODO */
            Method m_method; /**< TODO */
            ParamType1 m_param1; /**< TODO */
            /**
             * @brief
             *
             */
            void _Execute() { (m_object->*m_method)(m_param1); }

        public:

            /**
             * @brief
             *
             * @param object
             * @param method
             * @param param1
             */
            _Callback(Class* object, Method method, ParamType1 param1)
                : m_object(object), m_method(method),
                  m_param1(param1)
            {
            }

            /**
             * @brief
             *
             * @param const_Callback<Class
             * @param cb
             */
            _Callback(_Callback<Class, ParamType1> const& cb)
                : m_object(cb.m_object), m_method(cb.m_method),
                  m_param1(cb.m_param1)
            {
            }
    };

    template<class Class>
    /**
     * @brief
     *
     */
    class _Callback<Class>
    {
        protected:

            /**
             * @brief
             *
             */
            typedef void (Class::*Method)();
            Class* m_object; /**< TODO */
            Method m_method; /**< TODO */
            /**
             * @brief
             *
             */
            void _Execute() { (m_object->*m_method)(); }

        public:
            /**
             * @brief
             *
             * @param object
             * @param method
             */
            _Callback(Class* object, Method method)
                : m_object(object), m_method(method)
            {
            }
            /**
             * @brief
             *
             * @param cb
             */
            _Callback(_Callback<Class> const& cb)
                : m_object(cb.m_object), m_method(cb.m_method)
            {
            }
    };

    /// ---- Statics ----

    template < typename ParamType1 = void, typename ParamType2 = void, typename ParamType3 = void, typename ParamType4 = void >
    /**
     * @brief
     *
     */
    class _SCallback
    {
        protected:

            /**
             * @brief
             *
             */
            typedef void (*Method)(ParamType1, ParamType2, ParamType3, ParamType4);
            Method m_method; /**< TODO */
            ParamType1 m_param1; /**< TODO */
            ParamType2 m_param2; /**< TODO */
            ParamType3 m_param3; /**< TODO */
            ParamType4 m_param4; /**< TODO */
            /**
             * @brief
             *
             */
            void _Execute() { (*m_method)(m_param1, m_param2, m_param3, m_param4); }

        public:

            /**
             * @brief
             *
             * @param method
             * @param param1
             * @param param2
             * @param param3
             * @param param4
             */
            _SCallback(Method method, ParamType1 param1, ParamType2 param2, ParamType3 param3, ParamType4 param4)
                : m_method(method),
                  m_param1(param1), m_param2(param2), m_param3(param3), m_param4(param4)
            {
            }

            /**
             * @brief
             *
             * @param const_SCallback<ParamType1
             * @param ParamType2
             * @param ParamType3
             * @param cb
             */
            _SCallback(_SCallback<ParamType1, ParamType2, ParamType3, ParamType4> const& cb)
                : m_method(cb.m_method),
                  m_param1(cb.m_param1), m_param2(cb.m_param2), m_param3(cb.m_param3), m_param4(cb.m_param4)
            {
            }
    };

    template<typename ParamType1, typename ParamType2, typename ParamType3>
    /**
     * @brief
     *
     */
    class _SCallback<ParamType1, ParamType2, ParamType3>
    {
        protected:

            /**
             * @brief
             *
             */
            typedef void (*Method)(ParamType1, ParamType2, ParamType3);
            Method m_method; /**< TODO */
            ParamType1 m_param1; /**< TODO */
            ParamType2 m_param2; /**< TODO */
            ParamType3 m_param3; /**< TODO */
            /**
             * @brief
             *
             */
            void _Execute() { (*m_method)(m_param1, m_param2, m_param3); }

        public:
            /**
             * @brief
             *
             * @param method
             * @param param1
             * @param param2
             * @param param3
             */
            _SCallback(Method method, ParamType1 param1, ParamType2 param2, ParamType3 param3)
                : m_method(method),
                  m_param1(param1), m_param2(param2), m_param3(param3)
            {
            }
            /**
             * @brief
             *
             * @param const_SCallback<ParamType1
             * @param ParamType2
             * @param cb
             */
            _SCallback(_SCallback<ParamType1, ParamType2, ParamType3> const& cb)
                : m_method(cb.m_method),
                  m_param1(cb.m_param1), m_param2(cb.m_param2), m_param3(cb.m_param3)
            {
            }
    };

    template<typename ParamType1, typename ParamType2>
    /**
     * @brief
     *
     */
    class _SCallback<ParamType1, ParamType2>
    {
        protected:

            /**
             * @brief
             *
             */
            typedef void (*Method)(ParamType1, ParamType2);
            Method m_method; /**< TODO */
            ParamType1 m_param1; /**< TODO */
            ParamType2 m_param2; /**< TODO */
            /**
             * @brief
             *
             */
            void _Execute() { (*m_method)(m_param1, m_param2); }

        public:
            /**
             * @brief
             *
             * @param method
             * @param param1
             * @param param2
             */
            _SCallback(Method method, ParamType1 param1, ParamType2 param2)
                : m_method(method),
                  m_param1(param1), m_param2(param2)
            {
            }

            /**
             * @brief
             *
             * @param const_SCallback<ParamType1
             * @param cb
             */
            _SCallback(_SCallback<ParamType1, ParamType2> const& cb)
                : m_method(cb.m_method),
                  m_param1(cb.m_param1), m_param2(cb.m_param2)
            {
            }
    };

    template<typename ParamType1>
    /**
     * @brief
     *
     */
    class _SCallback<ParamType1>
    {
        protected:

            /**
             * @brief
             *
             */
            typedef void (*Method)(ParamType1);
            Method m_method; /**< TODO */
            ParamType1 m_param1; /**< TODO */
            /**
             * @brief
             *
             */
            void _Execute() { (*m_method)(m_param1); }

        public:
            /**
             * @brief
             *
             * @param method
             * @param param1
             */
            _SCallback(Method method, ParamType1 param1)
                : m_method(method),
                  m_param1(param1)
            {
            }

            /**
             * @brief
             *
             * @param cb
             */
            _SCallback(_SCallback<ParamType1> const& cb)
                : m_method(cb.m_method),
                  m_param1(cb.m_param1)
            {
            }
    };

    template<>
    /**
     * @brief
     *
     */
    class _SCallback<>
    {
        protected:

            /**
             * @brief
             *
             */
            typedef void (*Method)();
            Method m_method; /**< TODO */
            /**
             * @brief
             *
             */
            void _Execute() { (*m_method)(); }

        public:

            /**
             * @brief
             *
             * @param method
             */
            _SCallback(Method method)
                : m_method(method)
            {
            }

            /**
             * @brief
             *
             * @param cb
             */
            _SCallback(_SCallback<> const& cb)
                : m_method(cb.m_method)
            {
            }
    };
}

/// --------- GENERIC CALLBACKS ----------

namespace MaNGOS
{
    /**
     * @brief
     *
     */
    class ICallback
    {
        public:

            /**
             * @brief
             *
             */
            virtual void Execute() = 0;
            /**
             * @brief
             *
             */
            virtual ~ICallback() {}
    };

    template<class CB>
    /**
     * @brief
     *
     */
    class _ICallback : public CB, public ICallback
    {
        public:

            /**
             * @brief
             *
             * @param cb
             */
            _ICallback(CB const& cb) : CB(cb)
            {
            }

            /**
             * @brief
             *
             */
            void Execute() { CB::_Execute(); }
    };

    template < class Class, typename ParamType1 = void, typename ParamType2 = void, typename ParamType3 = void, typename ParamType4 = void >
    /**
     * @brief
     *
     */
    class Callback : public _ICallback<_Callback<Class, ParamType1, ParamType2, ParamType3, ParamType4> >
    {
        private:

            /**
             * @brief
             *
             */
            typedef _Callback<Class, ParamType1, ParamType2, ParamType3, ParamType4> C4;
        public:

            /**
             * @brief
             *
             * @param object
             * @param method
             * @param param1
             * @param param2
             * @param param3
             * @param param4
             */
            Callback(Class* object, typename C4::Method method, ParamType1 param1, ParamType2 param2, ParamType3 param3, ParamType4 param4)
                : _ICallback<C4>(C4(object, method, param1, param2, param3, param4))
            {
            }
    };

    template<class Class, typename ParamType1, typename ParamType2, typename ParamType3>
    /**
     * @brief
     *
     */
    class Callback<Class, ParamType1, ParamType2, ParamType3> : public _ICallback<_Callback<Class, ParamType1, ParamType2, ParamType3> >
    {
        private:

            /**
             * @brief
             *
             */
            typedef _Callback<Class, ParamType1, ParamType2, ParamType3> C3;
        public:

            /**
             * @brief
             *
             * @param object
             * @param method
             * @param param1
             * @param param2
             * @param param3
             */
            Callback(Class* object, typename C3::Method method, ParamType1 param1, ParamType2 param2, ParamType3 param3)
                : _ICallback<C3>(C3(object, method, param1, param2, param3))
            {
            }
    };

    template<class Class, typename ParamType1, typename ParamType2>
    /**
     * @brief
     *
     */
    class Callback<Class, ParamType1, ParamType2> : public _ICallback<_Callback<Class, ParamType1, ParamType2> >
    {
        private:

            /**
             * @brief
             *
             */
            typedef _Callback<Class, ParamType1, ParamType2> C2;

        public:
            /**
             * @brief
             *
             * @param object
             * @param method
             * @param param1
             * @param param2
             */
            Callback(Class* object, typename C2::Method method, ParamType1 param1, ParamType2 param2)
                : _ICallback<C2>(C2(object, method, param1, param2))
            {
            }
    };

    template<class Class, typename ParamType1>
    /**
     * @brief
     *
     */
    class Callback<Class, ParamType1> : public _ICallback<_Callback<Class, ParamType1> >
    {
        private:

            /**
             * @brief
             *
             */
            typedef _Callback<Class, ParamType1> C1;

        public:

            /**
             * @brief
             *
             * @param object
             * @param method
             * @param param1
             */
            Callback(Class* object, typename C1::Method method, ParamType1 param1)
                : _ICallback<C1>(C1(object, method, param1))
            {
            }
    };

    template<class Class>
    /**
     * @brief
     *
     */
    class Callback<Class> : public _ICallback<_Callback<Class> >
    {
        private:

            /**
             * @brief
             *
             */
            typedef _Callback<Class> C0;

        public:

            /**
             * @brief
             *
             * @param object
             * @param method
             */
            Callback(Class* object, typename C0::Method method)
                : _ICallback<C0>(C0(object, method))
            {
            }
    };
}

/// ---------- QUERY CALLBACKS -----------

class QueryResult;

namespace MaNGOS
{
    /**
     * @brief
     *
     */
    class IQueryCallback
    {
        public:

            /**
             * @brief
             *
             */
            virtual void Execute() = 0;
            /**
             * @brief
             *
             */
            virtual ~IQueryCallback() {}
            /**
             * @brief
             *
             * @param result
             */
            virtual void SetResult(QueryResult* result) = 0;
            /**
             * @brief
             *
             * @return QueryResult
             */
            virtual QueryResult* GetResult() = 0;
    };

    template<class CB>
    /**
     * @brief
     *
     */
    class _IQueryCallback : public CB, public IQueryCallback
    {
        public:

            /**
             * @brief
             *
             * @param cb
             */
            _IQueryCallback(CB const& cb) : CB(cb)
            {
            }

            /**
             * @brief
             *
             */
            void Execute() { CB::_Execute(); }
            /**
             * @brief
             *
             * @param result
             */
            void SetResult(QueryResult* result) { CB::m_param1 = result; }
            /**
             * @brief
             *
             * @return QueryResult
             */
            QueryResult* GetResult() { return CB::m_param1; }
    };

    template < class Class, typename ParamType1 = void, typename ParamType2 = void, typename ParamType3 = void >
    /**
     * @brief
     *
     */
    class QueryCallback : public _IQueryCallback<_Callback<Class, QueryResult*, ParamType1, ParamType2, ParamType3> >
    {
        private:

            /**
             * @brief
             *
             */
            typedef _Callback<Class, QueryResult*, ParamType1, ParamType2, ParamType3> QC3;

        public:

            /**
             * @brief
             *
             * @param object
             * @param method
             * @param result
             * @param param1
             * @param param2
             * @param param3
             */
            QueryCallback(Class* object, typename QC3::Method method, QueryResult* result, ParamType1 param1, ParamType2 param2, ParamType3 param3)
                : _IQueryCallback<QC3>(QC3(object, method, result, param1, param2, param3))
            {
            }
    };

    template<class Class, typename ParamType1, typename ParamType2>
    /**
     * @brief
     *
     */
    class QueryCallback<Class, ParamType1, ParamType2> : public _IQueryCallback<_Callback<Class, QueryResult*, ParamType1, ParamType2> >
    {
        private:

            /**
             * @brief
             *
             */
            typedef _Callback<Class, QueryResult*, ParamType1, ParamType2> QC2;

        public:

            /**
             * @brief
             *
             * @param object
             * @param method
             * @param result
             * @param param1
             * @param param2
             */
            QueryCallback(Class* object, typename QC2::Method method, QueryResult* result, ParamType1 param1, ParamType2 param2)
                : _IQueryCallback<QC2>(QC2(object, method, result, param1, param2))
            {
            }
    };

    template<class Class, typename ParamType1>
    /**
     * @brief
     *
     */
    class QueryCallback<Class, ParamType1> : public _IQueryCallback<_Callback<Class, QueryResult*, ParamType1> >
    {
        private:

            /**
             * @brief
             *
             */
            typedef _Callback<Class, QueryResult*, ParamType1> QC1;

        public:

            /**
             * @brief
             *
             * @param object
             * @param method
             * @param result
             * @param param1
             */
            QueryCallback(Class* object, typename QC1::Method method, QueryResult* result, ParamType1 param1)
                : _IQueryCallback<QC1>(QC1(object, method, result, param1))
            {
            }
    };

    template<class Class>
    /**
     * @brief
     *
     */
    class QueryCallback<Class> : public _IQueryCallback<_Callback<Class, QueryResult*> >
    {
        private:

            /**
             * @brief
             *
             */
            typedef _Callback<Class, QueryResult*> QC0;

        public:
            /**
             * @brief
             *
             * @param object
             * @param method
             * @param result
             */
            QueryCallback(Class* object, typename QC0::Method method, QueryResult* result)
                : _IQueryCallback<QC0>(QC0(object, method, result))
            {
            }
    };

    /// ---- Statics ----

    template < typename ParamType1 = void, typename ParamType2 = void, typename ParamType3 = void >
    /**
     * @brief
     *
     */
    class SQueryCallback : public _IQueryCallback<_SCallback<QueryResult*, ParamType1, ParamType2, ParamType3> >
    {
        private:

            /**
             * @brief
             *
             */
            typedef _SCallback<QueryResult*, ParamType1, ParamType2, ParamType3> QC3;

        public:

            /**
             * @brief
             *
             * @param method
             * @param result
             * @param param1
             * @param param2
             * @param param3
             */
            SQueryCallback(typename QC3::Method method, QueryResult* result, ParamType1 param1, ParamType2 param2, ParamType3 param3)
                : _IQueryCallback<QC3>(QC3(method, result, param1, param2, param3))
            {
            }
    };

    template<typename ParamType1, typename ParamType2>
    /**
     * @brief
     *
     */
    class SQueryCallback < ParamType1, ParamType2 > : public _IQueryCallback<_SCallback<QueryResult*, ParamType1, ParamType2> >
    {
        private:

            /**
             * @brief
             *
             */
            typedef _SCallback<QueryResult*, ParamType1, ParamType2> QC2;

        public:

            /**
             * @brief
             *
             * @param method
             * @param result
             * @param param1
             * @param param2
             */
            SQueryCallback(typename QC2::Method method, QueryResult* result, ParamType1 param1, ParamType2 param2)
                : _IQueryCallback<QC2>(QC2(method, result, param1, param2))
            {
            }
    };

    template<typename ParamType1>
    /**
     * @brief
     *
     */
    class SQueryCallback<ParamType1> : public _IQueryCallback<_SCallback<QueryResult*, ParamType1> >
    {
        private:

            /**
             * @brief
             *
             */
            typedef _SCallback<QueryResult*, ParamType1> QC1;

        public:

            /**
             * @brief
             *
             * @param method
             * @param result
             * @param param1
             */
            SQueryCallback(typename QC1::Method method, QueryResult* result, ParamType1 param1)
                : _IQueryCallback<QC1>(QC1(method, result, param1))
            {
            }
    };

    template<>
    /**
     * @brief
     *
     */
    class SQueryCallback<> : public _IQueryCallback<_SCallback<QueryResult*> >
    {
        private:

            /**
             * @brief
             *
             */
            typedef _SCallback<QueryResult*> QC0;

        public:

            /**
             * @brief
             *
             * @param method
             * @param result
             */
            SQueryCallback(QC0::Method method, QueryResult* result)
                : _IQueryCallback<QC0>(QC0(method, result))
            {
            }
    };
}

#endif
