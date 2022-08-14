#pragma once

#ifdef USE_FASTL

#include "../fastl/vector.h"

namespace fastl
{
	//------------------------------------------------------------------------------------------

    // TODO: could make these macros instead to increase debug build speed
	template<typename TChar>
    size_t ComputeStrLen(const TChar* str) // strlen
	{
		size_t ret; 
		for (ret = 0u; str[ret] != '\0';++ret){}
		return ret; 
	} 

	//------------------------------------------------------------------------------------------
	template<typename TChar>
    int ComputeStrCmp(const TChar* a, const TChar* b) // strcmp
	{
		for (size_t i = 0; ;++i)
		{
            // This also works for utf8
			if (a[i] != b[i])
                return a[i] < b[i] ? -1 : 1;
			if (a[i] == '\0')
                return 0;
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////////
	// Build string as a vector<char>
	template<typename TChar>
	class StringImpl
	{
	private:
		typedef vector<TChar> TData;
	public:
		typedef TChar value_type;
		typedef typename TData::size_type size_type;

		static constexpr size_type npos = -1;
	public:
		StringImpl();
		StringImpl(const TChar* input);
		StringImpl(const TChar* input, size_type length);

		void clear();

		bool empty() const { return size() == 0u; }
		size_type size() const { return m_data.empty() ? 0 : m_data.size() - 1; }
		size_type length() const { return size(); }

        TChar* begin() { return m_data.begin(); }
		const TChar* begin() const { return m_data.begin(); }
        
        // this exludes the \0
        TChar* end() { return m_data.end() - 1; }
		const TChar* end() const { return m_data.end() - 1; }

        TChar front() const { return *begin(); }
        TChar back() const { return *end(); }
        
        const value_type* c_str() const { return m_data.begin(); }

        TChar& operator[](size_type index) { return m_data[index]; }
        TChar  operator[](size_type index) const { return m_data[index]; }

		StringImpl& erase(size_type index){ m_data.erase(m_data.begin()+index); return *this; }
		StringImpl& erase(size_type index, size_type count){ m_data.erase(m_data.begin()+index,m_data.begin()+index+count); return *this; }

		void append(const TChar* str );

		StringImpl<TChar> operator+(const TChar c);
		StringImpl<TChar> operator+(const TChar* str);
		StringImpl<TChar> operator+(const StringImpl<TChar>& str);

        StringImpl<TChar>& operator += (TChar c) { m_data.insert(m_data.end()-1,c); return *this; }
        StringImpl<TChar>& operator += (const TChar* str) { Append(str,ComputeStrLen(str)); return *this; }
        StringImpl<TChar>& operator += (const StringImpl<TChar>& str) { Append(str.c_str(), str.size()); return *this; }

		bool operator == (const TChar* str) const { return ComputeStrCmp(c_str(), str) == 0; }
		bool operator != (const TChar* str) const { return ComputeStrCmp(c_str(), str) != 0; }
		bool operator <  (const TChar* str) const { return ComputeStrCmp(c_str(), str) < 0; }
		bool operator >  (const TChar* str) const { return ComputeStrCmp(c_str(), str) > 0; }

		bool operator == (const StringImpl<TChar>& str) const { return *this == str.c_str(); }
		bool operator != (const StringImpl<TChar>& str) const { return *this != str.c_str(); }
		bool operator <  (const StringImpl<TChar>& str) const { return *this < str.c_str(); }
		bool operator >  (const StringImpl<TChar>& str) const { return *this > str.c_str(); }

        bool find_last_of(TChar c)
        {
            return strrchr(m_data.data(), c);
        }
        
        StringImpl<TChar> substr(size_type start, size_type count)
        {
            return StringImpl<TChar>(&m_data[start], count);
        }
        
        void pop_back()
        {
            if (!empty())
            {
                // This doesn't work for multibyte chars
                m_data.pop_back();
                m_data[m_data.size()-1] = (TChar)0;
            }
        }
        
        void insert(size_type index, const TChar* str)
        {
            size_type len = ComputeStrLen(str);
            m_data.insert(m_data.begin()+index, str, str+len);
        }
        
        void resize(size_type size, TChar value = 0)
        {
            size_type oldSize = m_data.size();
            size_type newSize = size+1;
            if (newSize == oldSize)
                return;
            
            m_data.resize(newSize);
            
            // Note: length and strlen with value of 0 unless those chars are filled
            if (newSize > oldSize)
            {
                for (uint32_t i = oldSize-1; i < newSize; ++i)
                {
                    m_data[i] = value;
                }
            }
            m_data[newSize-1] = 0;
        }
        
	private: 
		void Append(const TChar* str, const size_type appendSize);

	private: 
		TData m_data;
	};

	//Implementation

	//------------------------------------------------------------------------------------------
	template<typename TChar>
    StringImpl<TChar>::StringImpl()
	{
        // TODO: this requires a heap allocate for all empty strings
        m_data.reserve(1);
		clear();
	}

	//------------------------------------------------------------------------------------------
	template<typename TChar>
    StringImpl<TChar>::StringImpl(const TChar* input)
	{ 
        size_t length = ComputeStrLen(input);
        m_data.reserve(length + 1);
        clear();
        Append(input, length);
	}

	//------------------------------------------------------------------------------------------
	template<typename TChar>
    StringImpl<TChar>::StringImpl(const TChar* input, const size_type length)
	{
        m_data.reserve(length + 1);
		clear();
		Append(input, length);
	}

	//------------------------------------------------------------------------------------------
	template<typename TChar>
    inline void StringImpl<TChar>::clear()
	{
        // need small string optimization
		m_data.resize(1); 
		m_data[0] = '\0'; 
	}

	//------------------------------------------------------------------------------------------
	template<typename TChar>
    void StringImpl<TChar>::append( const TChar* str )
	{
        Append(str, ComputeStrLen(str));
	}

	//------------------------------------------------------------------------------------------
	template<typename TChar>
    StringImpl<TChar> StringImpl<TChar>::operator+(TChar c)
	{ 
        StringImpl<TChar> ret;
        ret.reserve(m_data.size() + 1);
        
        char cstr[2] = { c, 0 };
        ret.Append(c_str(), size());
        ret.Append(cstr, 1);
		return ret;
	}
	//------------------------------------------------------------------------------------------
	template<typename TChar>
    StringImpl<TChar> StringImpl<TChar>::operator+(const TChar* str)
	{ 
		StringImpl<TChar> ret;
        size_t len = ComputeStrLen(str);
        ret.reserve(m_data.size() + len);

        ret.Append(c_str(), size());
        ret.Append(str, len);
		return ret; 
	} 

	//------------------------------------------------------------------------------------------
	template<typename TChar>
    StringImpl<TChar> StringImpl<TChar>::operator+(const StringImpl<TChar>& str)
	{ 
        StringImpl<TChar> ret;
        size_t len = str.size();
        ret.reserve(m_data.size() + len);

        ret.Append(c_str(), size());
        ret.Append(str, len);
        return ret;
	}

	//------------------------------------------------------------------------------------------
	template<typename TChar>
    void StringImpl<TChar>::Append(const TChar* str, const size_type appendSize)
	{ 
		size_type writeIndex = size();
		m_data.resize(m_data.size()+appendSize);
		for (size_type i = 0; i < appendSize; ++i, ++writeIndex)
		{
			m_data[writeIndex] = str[i];
		}
		m_data.back() = '\0';
	}	

	using string = StringImpl<char>;
	
    // Code above is using char* in many places instead of TChar
    // TODO: elim wstring if possible
    // using wstring = StringImpl<wchar_t>;
}

#else

//#include <string>
//
//namespace fastl
//{
//  using string = std::string;
//  using wstring = std::wstring;
//}

#endif //USE_FASTL

#ifdef FASTL_EXPOSE_PLAIN_ALIAS

using string = fastl::string;
using wstring = fastl::wstring;

#endif //FASTL_EXPOSE_PLAIN_ALIAS
