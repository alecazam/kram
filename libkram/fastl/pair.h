#pragma once 

#ifdef USE_FASTL

namespace fastl
{
	template<typename T1, typename T2>
	struct pair
	{
		typedef T1 first_type;
		typedef T2 second_type;

		pair()
            :first(),second(){}
        pair(const T1& _first, const T2& _second)
            :first(_first),second(_second) {}

        // added these
        pair(const pair<T1, T2>& rhs)
            :first(rhs.first),second(rhs.second) {}
        pair(pair<T1, T2>&& rhs)
            :first(std::move(rhs.first)),second(std::move(rhs.second)) {}
		
        pair<T1,T2>& operator=(const pair<T1, T2>& rhs)
        {
            first = rhs.first;
            second = rhs.second;
            return *this;
        }
        pair<T1,T2>& operator=(pair<T1, T2>&& rhs)
        {
            first = std::move(rhs.first);
            second = std::move(rhs.second);
            return *this;
        }
        
		T1 first; 
		T2 second;
	};

    template<typename T1, typename T2>
    pair<T1,T2> make_pair(const T1& k, const T2& v) { return pair(k,v); }
}

#else 

//#include <utility>
//
//namespace fastl
//{
//	template<typename TFirst, typename TSecond> using pair = std::pair<TFirst, TSecond>;
//}

#endif //USE_FASTL

#ifdef FASTL_EXPOSE_PLAIN_ALIAS

template<typename TFirst,typename TSecond> using pair = fastl::pair<TFirst,TSecond>;

#endif //FASTL_EXPOSE_PLAIN_ALIAS
