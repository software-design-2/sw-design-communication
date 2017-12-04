// Copyright 2016 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <fastcdr/Cdr.h>
#include <fastcdr/exceptions/BadParamException.h>

using namespace eprosima::fastcdr;
using namespace ::exception;

#if __BIG_ENDIAN__
const Cdr::Endianness Cdr::DEFAULT_ENDIAN = BIG_ENDIANNESS;
#else
const Cdr::Endianness Cdr::DEFAULT_ENDIAN = LITTLE_ENDIANNESS;
#endif

CONSTEXPR size_t ALIGNMENT_LONG_DOUBLE = 8;

Cdr::state::state(const Cdr &cdr) : m_currentPosition(cdr.m_currentPosition), m_alignPosition(cdr.m_alignPosition),
    m_swapBytes(cdr.m_swapBytes), m_lastDataSize(cdr.m_lastDataSize) {}

    Cdr::state::state(const state &state) : m_currentPosition(state.m_currentPosition), m_alignPosition(state.m_alignPosition),
    m_swapBytes(state.m_swapBytes), m_lastDataSize(state.m_lastDataSize) {}

Cdr::Cdr(FastBuffer &cdrBuffer, const Endianness endianness, const CdrType cdrType) : m_cdrBuffer(cdrBuffer),
    m_cdrType(cdrType), m_plFlag(DDS_CDR_WITHOUT_PL), m_options(0), m_endianness((uint8_t)endianness),
    m_swapBytes(endianness == DEFAULT_ENDIAN ? false : true), m_lastDataSize(0), m_currentPosition(cdrBuffer.begin()),
    m_alignPosition(cdrBuffer.begin()), m_lastPosition(cdrBuffer.end())
{
}

// @brief 读取CDR流的封装（如果CDR流有一个封装，则在反序列化之前调用这个函数）
// @param
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception::BadParamException 当要反序列化一个无效的值的时候这个异常被抛出
Cdr& Cdr::read_encapsulation()
{

    uint8_t dummy = 0, encapsulationKind = 0;
    //保存当前的CDR的状态为state
    state state(*this);

    try
    {
        // If it is DDS_CDR, the first step is to get the dummy byte.
        //如果是DDS_CDR类型，第一步就是获取虚字节
        if(m_cdrType == DDS_CDR)
        {
            //反序列化
            (*this) >> dummy;
        }

        // Get the ecampsulation byte.
        (*this) >> encapsulationKind;


        // If it is a different endianness, make changes.
        //如果不是正常的字节序，那么设置m_swapBytes标志，并且改变m_endianness
        if(m_endianness != (encapsulationKind & 0x1))
        {
            m_swapBytes = !m_swapBytes;
            m_endianness = (encapsulationKind & 0x1);
        }
    }
    //如果没有成功反序列化，抛出异常，并且重置CDR序列化的状态
    catch(Exception &ex)
    {
        setState(state);
        ex.raise();
    }

    // If it is DDS_CDR type, view if contains a parameter list.
    //如果是一个DDS_CDR类型，看是否有一个参数列表
    if(encapsulationKind & DDS_CDR_WITH_PL)
    {
        //DDS_CDR_WITH_PL=0x2
        if(m_cdrType == DDS_CDR)
        {
            m_plFlag = DDS_CDR_WITH_PL;
        }
        else
        {
            //抛出异常
            throw BadParamException("Unexpected CDR type received in Cdr::read_encapsulation");
        }
    }

    try
    {
        if(m_cdrType == DDS_CDR)
            (*this) >> m_options;
    }
    catch(Exception &ex)
    {
        setState(state);
        ex.raise();
    }

    resetAlignment();
    return *this;
}


// @brief 读取CDR流的封装（如果CDR流有一个封装，则在序列化之前调用这个函数）
// @param
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception::BadParamException 当要反序列化一个无效的值的时候这个异常被抛出
Cdr& Cdr::serialize_encapsulation()
{
    uint8_t dummy = 0, encapsulationKind = 0;
    //保存当前state
    state state(*this);

    try
    {
        // If it is DDS_CDR, the first step is to serialize the dummy byte.
        //如果是DDS_CDR类型，第一步是序列化dummy byte
        if(m_cdrType == DDS_CDR)
        {
            (*this) << dummy;
        }

        // Construct encapsulation byte.
        //
        encapsulationKind = ((uint8_t)m_plFlag | m_endianness);

        // Serialize the encapsulation byte.
        (*this) << encapsulationKind;
    }
    //捕捉异常并且还原状态state
    catch(Exception &ex)
    {
        setState(state);
        ex.raise();
    }

    try
    {
        if(m_cdrType == DDS_CDR)
            (*this) << m_options;
    }
    catch(Exception &ex)
    {
        setState(state);
        ex.raise();
    }

    resetAlignment();
    return *this;
}

// @brief 该函数返回DDS_CDR的m_plFlag
// @param
// @return Cdr::DDSCdrPlFlag
// @exception
Cdr::DDSCdrPlFlag Cdr::getDDSCdrPlFlag() const
{
    return m_plFlag;
}

// @brief 该函数返回DDS_CDR的option flag
// @param
// @return options
// @exception
void Cdr::setDDSCdrPlFlag(DDSCdrPlFlag plFlag)
{
    m_plFlag = plFlag;
}

// @brief 该函数返回DDS_CDR的option flag
// @param
// @return options
// @exception
uint16_t Cdr::getDDSCdrOptions() const
{
    return m_options;
}

// @brief 该函数用于设置DDS_CDR的option flag
// @param options要设置的标志
// @return
// @exception
void Cdr::setDDSCdrOptions(uint16_t options)
{
    m_options = options;
}

// @brief 该函数用于缓冲区跳过numBytes个字节
// @param size_t numBytes 要跳过的字节数
// @return bool false表明此函数没有跳过指定的字节数，true表明正确跳过了指定的字节数
// @exception
void Cdr::changeEndianness(Endianness endianness)
{
    if(m_endianness != endianness)
    {
        m_swapBytes = !m_swapBytes;
        m_endianness = endianness;
    }
}

// @brief 该函数用于缓冲区跳过numBytes个字节
// @param size_t numBytes 要跳过的字节数
// @return bool false表明此函数没有跳过指定的字节数，true表明正确跳过了指定的字节数
// @exception
bool Cdr::jump(size_t numBytes)
{
    bool returnedValue = false;
    //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= numBytes) || resize(numBytes))
    {
        //将缓冲区的现在的位置加上要跳过的字节数numBytes
        m_currentPosition += numBytes;
        //正确跳过之后设置returnedValue为true表明正确执行了
        returnedValue = true;
    }
    //返回
    return returnedValue;
}

// @brief 该函数找到当前使用的缓冲区的指针
// @param
// @return char* 缓冲区的指针
// @exception
char* Cdr::getBufferPointer()
{
    return m_cdrBuffer.getBuffer();
}

// @brief 返回当前CDR流的位置
// @param
// @return char* 位置的指针
// @exception
char* Cdr::getCurrentPosition()
{
    return &m_currentPosition;
}


// @brief 返回序列化的状态
// @param
// @return Cdr::state 序列化状态
// @exception
Cdr::state Cdr::getState()
{
    return Cdr::state(*this);
}

// @brief 设置CDR序列化过程的状态（设置成以前的状态）
// @param 将要设置成的状态
// @return
// @exception
void Cdr::setState(state &state)
{
    //赋值的过程
    m_currentPosition >> state.m_currentPosition;
    m_alignPosition >> state.m_alignPosition;
    m_swapBytes = state.m_swapBytes;
    m_lastDataSize = state.m_lastDataSize;
}

// @brief 将缓冲区重置为初始位置
// @param
// @return
// @exception
void Cdr::reset()
{
    //当前的位置回到缓冲区开始的地方
    m_currentPosition = m_cdrBuffer.begin();
    //对其的位置回到缓冲区开始的地方
    m_alignPosition = m_cdrBuffer.begin();
    //如果m_endianness是默认字节顺序，则m_swapBytes为false，否则为true
    m_swapBytes = m_endianness == DEFAULT_ENDIAN ? false : true;
    //最新的datasize为0
    m_lastDataSize = 0;
}

// @brief 移动到字节对齐的位置
// @param size_t numBytes 需要移动的字节数
// @return bool 目前的对齐位置是否成功更新，false表明此操作失败，true表示成功
// @exception
bool Cdr::moveAlignmentForward(size_t numBytes)
{
    //初始化成功操作的标志为false
    bool returnedValue = false;

    //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_alignPosition) >= numBytes) || resize(numBytes))
    {
        //将对齐的位置加上numBytes
        m_alignPosition += numBytes;
        //设置成功的标志为true，表明移动的操作正确执行
        returnedValue = true;
    }
    //返回
    return returnedValue;
}

// @brief 申请内存
// @param size_t numBytes 需要移动的字节数
// @return bool 目前的对齐位置是否成功更新，false表明此操作失败，true表示成功
// @exception
bool Cdr::resize(size_t minSizeInc)
{
    //如果缓冲区可以申请到minSizeInc的空间
    if(m_cdrBuffer.resize(minSizeInc))
    {
        //???????????????????????
        m_currentPosition << m_cdrBuffer.begin();
        m_alignPosition << m_cdrBuffer.begin();
        m_lastPosition = m_cdrBuffer.end();
        return true;
    }
    return false;
}

//下面的都结束了

// @brief 该函数用于在机器的默认字节顺序下序列化一个char变量
// @param char_t 将在缓冲区中序列化的char变量的值
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const char char_t)
{
    //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= sizeof(char_t)) || resize(sizeof(char_t)))
    {
        // Save last datasize.
        //存储最新的数据大小
        m_lastDataSize = sizeof(char_t);
        //将chart型数据写入缓冲区并同时改变m_currentPosition的位置
        m_currentPosition++ << char_t;
        return *this;
    }
    //抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}



// @brief 该函数用于在机器的默认字节顺序下序列化一个short变量
// @param short_t 将在缓冲区中序列化的short变量的值
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const int16_t short_t)
{
    //需要对齐的字节数
    size_t align = alignment(sizeof(short_t));
    //序列化该数据类型所需的空间大小包括对齐加自身的大小
    size_t sizeAligned = sizeof(short_t) + align;

    //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        // Save last datasize.
        //存储最新的数据大小
        m_lastDataSize = sizeof(short_t);

        // Align.
        //数据对齐
        makeAlign(align);

        //判断是否需要交换字节
        if(m_swapBytes)
        {
            //如果需要交换字节，将该short型变量转换成char数组
            //通过将该变量取地址后把该地址强制转换成一个char型制指针实现
            const char *dst = reinterpret_cast<const char*>(&short_t);

            //按从后向前的顺序将short型数据的每个字节写入缓冲区并同时改变m_currentPosition的位置
            m_currentPosition++ << dst[1];
            m_currentPosition++ << dst[0];
        }
        else
        {
            //如果不需要交换字节，则直接将该数据写入缓冲区
            m_currentPosition << short_t;
            //同时m_currentPosition的位置后移short大小
            m_currentPosition += sizeof(short_t);
        }

        return *this;
    }
    //抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

// @brief 该函数用于序列化有不同字节顺序的short型变量
// @param short_t 将在缓冲区中序列化的short变量的值
// @param endianness 在此short型变量序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const int16_t short_t, Endianness endianness)
{
    //临时变量，用于记录m_swapBytes
    bool auxSwap = m_swapBytes;
    //根据参数endianness以及机器的字节顺序确定最终的字节顺序
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用serialize(short_t)函数
        serialize(short_t);
        //还原m_swapBytes
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //抛出异常同时还原m_swapBytes
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

// @brief 该函数用于在机器的默认字节顺序下序列化一个long变量
// @param long_t 将在缓冲区中序列化的long变量的值
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const int32_t long_t)
{
    //需要对齐的字节数
    size_t align = alignment(sizeof(long_t));
    //序列化该数据类型所需的空间大小包括对齐加自身的大小
    size_t sizeAligned = sizeof(long_t) + align;

    //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        // Save last datasize.
        //存储最新的数据大小
        m_lastDataSize = sizeof(long_t);

        // Align.
        //数据对齐
        makeAlign(align);

        //判断是否需要交换字节
        if(m_swapBytes)
        {
            //如果需要交换字节，将该long型变量转换成char数组
            //通过将该变量取地址后把该地址强制转换成一个char型制指针实现
            const char *dst = reinterpret_cast<const char*>(&long_t);

            //按从后向前的顺序将long double型数据的每个字节写入缓冲区并同时改变m_currentPosition的位置
            m_currentPosition++ << dst[3];
            m_currentPosition++ << dst[2];
            m_currentPosition++ << dst[1];
            m_currentPosition++ << dst[0];
        }
        else
        {
            //如果不需要交换字节，则直接将该数据写入缓冲区
            m_currentPosition << long_t;
            //同时m_currentPosition的位置后移long大小
            m_currentPosition += sizeof(long_t);
        }

        return *this;
    }

    //抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

// @brief 该函数用于序列化有不同字节顺序的long型变量
// @param long_t 将在缓冲区中序列化的long变量的值
// @param endianness 在此long型变量序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const int32_t long_t, Endianness endianness)
{
    //临时变量，用于记录m_swapBytes
    bool auxSwap = m_swapBytes;
    //根据参数endianness以及机器的字节顺序确定最终的字节顺序
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用serialize(long_t)函数
        serialize(long_t);
        //还原m_swapBytes
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //抛出异常同时还原m_swapBytes
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

// @brief 该函数用于在机器的默认字节顺序下序列化一个int64_t变量
// @param longlong_t 将在缓冲区中序列化的ldouble变量的值
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const int64_t longlong_t)
{
    //需要对齐的字节数
    size_t align = alignment(sizeof(longlong_t));
    //序列化该数据类型所需的空间大小包括对齐加自身的大小
    size_t sizeAligned = sizeof(longlong_t) + align;

    //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        // Save last datasize.
        //存储最新的数据大小
        m_lastDataSize = sizeof(longlong_t);

        // Align.
        //数据对齐
        makeAlign(align);

        //判断是否需要交换字节
        if(m_swapBytes)
        {
            //如果需要交换字节，将该longlong型变量转换成char数组
            //通过将该变量取地址后把该地址强制转换成一个char型制指针实现
            const char *dst = reinterpret_cast<const char*>(&longlong_t);

            //按从后向前的顺序将long double型数据的每个字节写入缓冲区并同时改变m_currentPosition的位置
            m_currentPosition++ << dst[7];
            m_currentPosition++ << dst[6];
            m_currentPosition++ << dst[5];
            m_currentPosition++ << dst[4];
            m_currentPosition++ << dst[3];
            m_currentPosition++ << dst[2];
            m_currentPosition++ << dst[1];
            m_currentPosition++ << dst[0];
        }
        else
        {
             //如果不需要交换字节，则直接将该数据写入缓冲区
            m_currentPosition << longlong_t;
             //同时m_currentPosition的位置后移longlong大小
            m_currentPosition += sizeof(longlong_t);
        }
        //抛出异常
        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

// @brief 该函数用于序列化有不同字节顺序的longlong型变量
// @param longlong_t 将在缓冲区中序列化的double变量的值
// @param endianness 在此longlong型变量序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const int64_t longlong_t, Endianness endianness)
{
    //临时变量,用于记录m_swapBytes
    bool auxSwap = m_swapBytes;
    //根据参数endianness以及机器字节存储顺序确定是否需要交换字节
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用serialize(longlong_t)函数
        serialize(longlong_t);
        //还原m_swapBytes
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //抛出异常同时还原m_swapBytes
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

// @brief 该函数用于序列化float型变量
// @param float_t 将在缓冲区中序列化的float变量的值
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const float float_t)
{
    //需要对齐的字节数
    size_t align = alignment(sizeof(float_t));
    //序列化该数据类型所需的空间大小包括对齐加自身的大小
    size_t sizeAligned = sizeof(float_t) + align;

    //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        // Save last datasize.
        //存储最新的数据大小
        m_lastDataSize = sizeof(float_t);

        // Align.；
        //数据对齐
        makeAlign(align);

        //判断是否需要交换字节
        if(m_swapBytes)
        {
            //如果需要交换字节，将该long double型变量转换成char数组
            //通过将该变量取地址后把该地址强制转换成一个char型制指针实现
            const char *dst = reinterpret_cast<const char*>(&float_t);

            //按从后向前的顺序将float型数据的每个字节写入缓冲区并同时改变m_currentPosition的位置
            m_currentPosition++ << dst[3];
            m_currentPosition++ << dst[2];
            m_currentPosition++ << dst[1];
            m_currentPosition++ << dst[0];
        }
        else
        {
            //如果不需要交换字节，则直接将该数据写入缓冲区
            m_currentPosition << float_t;
            //同时m_currentPosition的位置后移float大小
            m_currentPosition += sizeof(float_t);
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

// @brief 该函数用于序列化有不同字节顺序的float型变量
// @param float_t 将在缓冲区中序列化的float变量的值
// @param endianness 在此double型变量序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const float float_t, Endianness endianness)
{
    //临时变量,用于记录m_swapBytes
    bool auxSwap = m_swapBytes;
    //根据参数endianness以及机器字节存储顺序确定是否需要交换字节
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用serialize(float_t)函数
        serialize(float_t);
        //还原m_swapBytes
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //抛出异常同时还原m_swapBytes
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

// @brief 该函数用于序列化double型变量
// @param double_t 将在缓冲区中序列化的double变量的值
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const double double_t)
{
    //需要对齐的字节数
    size_t align = alignment(sizeof(double_t));
    //序列化该数据类型所需的空间大小包括对齐加自身的大小
    size_t sizeAligned = sizeof(double_t) + align;

    //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        // Save last datasize.
        //存储最新的数据大小
        m_lastDataSize = sizeof(double_t);

        // Align.
        //数据对齐
        makeAlign(align);

        //判断是否需要交换字节
        if(m_swapBytes)
        {
            //如果需要交换字节，将该long double型变量转换成char数组
            //通过将该变量取地址后把该地址强制转换成一个char型制指针实现
            const char *dst = reinterpret_cast<const char*>(&double_t);

            //按从后向前的顺序将double型数据的每个字节写入缓冲区并同时改变m_currentPosition的位置
            m_currentPosition++ << dst[7];
            m_currentPosition++ << dst[6];
            m_currentPosition++ << dst[5];
            m_currentPosition++ << dst[4];
            m_currentPosition++ << dst[3];
            m_currentPosition++ << dst[2];
            m_currentPosition++ << dst[1];
            m_currentPosition++ << dst[0];
        }
        else
        {
            //如果不需要交换字节，则直接将该数据写入缓冲区
            m_currentPosition << double_t;
            //同时m_currentPosition的位置后移double大小
            m_currentPosition += sizeof(double_t);
        }
        return *this;
    }
    //抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

/*stop here WLZ*/
// @brief 该函数用于序列化有不同字节顺序的double型变量
// @param double_t 将在缓冲区中序列化的double变量的值
// @param endianness 在此double型变量序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const double double_t, Endianness endianness)
{   
    //临时变量,用于记录m_swapBytes
    bool auxSwap = m_swapBytes;
    //根据参数endianness以及机器字节存储顺序确定是否需要交换字节，即确定m_swapBytes的值
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用序列化函数
        serialize(double_t);
        //还原m_swapBytes
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //如果序列化失败，抛出异常同时还原m_swapBytes
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

// @brief 该函数用于在默认字节顺序下序列化一个long double变量
// @param ldouble_t 将在缓冲区中序列化的ldouble变量的值
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const long double ldouble_t)
{
    //需要对齐的字节数
    size_t align = alignment(ALIGNMENT_LONG_DOUBLE);
    //序列化该数据类型所需的空间大小包括对齐加自身的大小
    size_t sizeAligned = sizeof(ldouble_t) + align;

    //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        //存储最新的数据大小
        // Save last datasize.
        m_lastDataSize = sizeof(ldouble_t);

        //数据对齐
        // Align.
        makeAlign(align);

        //判断是否需要交换字节
        if(m_swapBytes)
        {   
            //如果需要交换字节，将该long double型变量转换成char数组
            //通过将该变量取地址后把该地址强制转换成一个char型制指针实现
            const char *dst = reinterpret_cast<const char*>(&ldouble_t);

            //按从后向前的顺序将long double型数据的每个字节写入缓冲区并同时更新m_currentPosition的位置
            m_currentPosition++ << dst[15];
            m_currentPosition++ << dst[14];
            m_currentPosition++ << dst[13];
            m_currentPosition++ << dst[12];
            m_currentPosition++ << dst[11];
            m_currentPosition++ << dst[10];
            m_currentPosition++ << dst[9];
            m_currentPosition++ << dst[8];
            m_currentPosition++ << dst[7];
            m_currentPosition++ << dst[6];
            m_currentPosition++ << dst[5];
            m_currentPosition++ << dst[4];
            m_currentPosition++ << dst[3];
            m_currentPosition++ << dst[2];
            m_currentPosition++ << dst[1];
            m_currentPosition++ << dst[0];
        }
        else
        {
            //如果不需要交换字节，则直接将该数据写入缓冲区
            m_currentPosition << ldouble_t;
            //同时m_currentPosition的位置后移long double大小
            m_currentPosition += sizeof(ldouble_t);
        }

        return *this;
    }

    //抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

// @brief 该函数用于序列化有不同字节顺序的long double型变量
// @param ldouble_t 将在缓冲区中序列化的long double变量的值
// @param endianness 在此double型变量序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const long double ldouble_t, Endianness endianness)
{
    //临时变量，用于记录m_swapBytes
    bool auxSwap = m_swapBytes;
    //根据参数endianness以及机器字节存储顺序确定是否需要交换字节，即确定m_swapBytes的值
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用已有的序列化函数
        serialize(ldouble_t);
        //还原m_swapBytes
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //如果序列化失败，抛出异常同时还原m_swapBytes
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

// @brief 该函数用于在默认字节顺序下序列化一个bool变量
// @param bool_t 将在缓冲区中序列化的bool变量的值
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const bool bool_t)
{
    
    //定义一个unsigned char临时变量用于序列化该bool型变量并给其赋初值0
    uint8_t value = 0;

  //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= sizeof(uint8_t)) || resize(sizeof(uint8_t)))
    {
        //保存最新的数据大小
        // Save last datasize.
        m_lastDataSize = sizeof(uint8_t);

        //如果该bool型变量为true，把临时变量value置为1
        if(bool_t)
            value = 1;
        //把临时变量写入缓冲区
        m_currentPosition++ << value;

        return *this;
    }

    //如果条件不满足则抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

// @brief 该函数用于在默认字节顺序下序列化一个字符串 即String
// @param string_t是一个指向将被序列化的字符串的指针
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const char *string_t)
{
    //临时变量，表示字符串长度
    uint32_t length = 0;

    //获取字符串的长度
    if(string_t != nullptr)
        length = (uint32_t)strlen(string_t) + 1;

    if(length > 0)
    {
        //如果字符串不为空

        //复制当前cdr序列化的状态
        Cdr::state state(*this);
        //首先将字符串的长度序列化
        serialize(length);

         //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
        if(((m_lastPosition - m_currentPosition) >= length) || resize(length))
        {
            //存储最新的数据大小
            // Save last datasize.
            m_lastDataSize = sizeof(uint8_t);

            //将字符串拷贝到缓冲区
            m_currentPosition.memcopy(string_t, length);
            //将m_currentPostion向后移动length
            m_currentPosition += length;
        }
        else
        {
            //如果没能成功序列化则使用前面复制的state重置CDR序列化的状态，并抛出异常
            setState(state);
            throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
        }
    }
    else
        //如果字符串为空则仅序列化其长度
        serialize(length);

    return *this;
}

// @brief 该函数用于根据给定的字节顺序序列化字符串，即String
// @param string_t 是一个指向将被序列化的字符串的指针
// @param endianness 在此字符串序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const char *string_t, Endianness endianness)
{

    //临时变量，用于记录m_swapBytes
    bool auxSwap = m_swapBytes;
    //根据参数endianness以及机器字节存储顺序确定是否需要交换字节，即确定m_swapBytes的值
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用已有的序列化函数
        serialize(string_t);
        //还原m_swapBytes
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //抛出异常同时还原m_swapBytes
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

// @brief 该函数序列化一个bool型数组
// @param bool_t 将要序列化的bool型数组
// @param numElements bool型数组元素的数量
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serializeArray(const bool *bool_t, size_t numElements)
{
    //该bool数组的占的空间
    size_t totalSize = sizeof(*bool_t)*numElements;

    //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= totalSize) || resize(totalSize))
    {
        //保存最新的数据大小，注意是bool型的大小而不是数组大小
        // Save last datasize.
        m_lastDataSize = sizeof(*bool_t);

        //循环将bool数组中的元素写入缓冲区，方法与序列化bool型变量的方法完全相同
        for(size_t count = 0; count < numElements; ++count)
        {
            uint8_t value = 0;

            if(bool_t[count])
                value = 1;
            m_currentPosition++ << value;
        }

        return *this;
    }

    //如果条件不满足则抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

// @brief 该函数序列化一个字符数组
// @param char_t 将要序列化的字符数组
// @param numElements bool型数组元素的数量
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serializeArray(const char *char_t, size_t numElements)
{
    //该字符数组所占的总空间
    size_t totalSize = sizeof(*char_t)*numElements;

    //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= totalSize) || resize(totalSize))
    {
        //保存最新的数据大小，即char型
        // Save last datasize.
        m_lastDataSize = sizeof(*char_t);

        //将chat_t拷贝到缓冲区中
        m_currentPosition.memcopy(char_t, totalSize);
        //更新缓冲区m_currentPosition的位置
        m_currentPosition += totalSize;
        return *this;
    }

    //条件不满足则抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

// @brief 该函数序列化一个short数组
// @param short_t 将要序列化的short数组
// @param numElements short数组元素的数量
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serializeArray(const int16_t *short_t, size_t numElements)
{   
    //需要对齐的字节数
    size_t align = alignment(sizeof(*short_t));
    //该short数组的字节总数
    size_t totalSize = sizeof(*short_t) * numElements;
    //序列化该数据类型所需的空间大小，包括对齐和数组本身的大小
    size_t sizeAligned = totalSize + align;

    //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        //储存最新的数据大小
        // Save last datasize.
        m_lastDataSize = sizeof(*short_t);

        //如果有元素则对齐数据
        // Align if there are any elements
        if(numElements)
            makeAlign(align);

        if(m_swapBytes)
        {
            //如果需要交换字节顺序，将short型数组转换为char型数组再序列化
            //dst为转换后的char指针
            const char *dst = reinterpret_cast<const char*>(&short_t);
            //end为short数组结束的位置
            const char *end = dst + totalSize;

            //遍历short数组的每一个元素，对于每一个short元素，从后向前把每一个字节写入缓冲区，并更新m_currentPosition的值
            for(; dst < end; dst += sizeof(*short_t))
            {
                m_currentPosition++ << dst[1];
                m_currentPosition++ << dst[0];
            }
        }
        else
        {
            //如果不需要交换字节，直接将数组写入缓冲区
            m_currentPosition.memcopy(short_t, totalSize);
            //同时更新m_currentPosition的值，向后偏移short数组大小
            m_currentPosition += totalSize;
        }

        return *this;
    }

    //抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

// @brief 该函数用于序列化有不同字节顺序short数组
// @param short_t 将要序列化的short数组
// @param numElements short数组元素的数量
// @param endianness 序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serializeArray(const int16_t *short_t, size_t numElements, Endianness endianness)
{
    //临时变量，用于记录m_swapBytes
    bool auxSwap = m_swapBytes;
    //根据参数endianness以及机器字节存储顺序确定是否需要交换字节，即确定m_swapBytes的值
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用已有的序列化函数
        serializeArray(short_t, numElements);
        //还原m_swapBytes
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //如果序列化失败则抛出异常并还原m_swapBytes
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

// @brief 该函数序列化一个int型数组(数组元素为4字节)
// @param long_t 将要序列化的int数组
// @param numElements int数组元素的数量
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serializeArray(const int32_t *long_t, size_t numElements)
{
    //需要对齐的字节数(仅与数组元素有关)
    size_t align = alignment(sizeof(*long_t));
    //int数组的字节总数
    size_t totalSize = sizeof(*long_t) * numElements;
    //序列化改数据所需要的空间大小，包括对齐和数据本身大小
    size_t sizeAligned = totalSize + align;

    //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        //存储最新数据的大小(仅仅是数组元素的大小，而不是数组本身大小)
        // Save last datasize.
        m_lastDataSize = sizeof(*long_t);

        //如果有元素则对齐数据
        // Align if there are any elements
        if(numElements)
            makeAlign(align);

        if(m_swapBytes)
        {
            //如果需要交换字节顺序，把int型数组转换为char型数组在序列化
            //dst为int数组转换后的char指针
            const char *dst = reinterpret_cast<const char*>(&long_t);
            //end为int数组结束的位置
            const char *end = dst + totalSize;

            //遍历int数组的元素，int的每一个字节从后向前写入缓冲区，同时更新m_currentPosition的值
            for(; dst < end; dst += sizeof(*long_t))
            {
                m_currentPosition++ << dst[3];
                m_currentPosition++ << dst[2];
                m_currentPosition++ << dst[1];
                m_currentPosition++ << dst[0];
            }
        }
        else
        {
            //如果不需要交换字节，直接把数组写入缓冲区
            m_currentPosition.memcopy(long_t, totalSize);
            //同时更新m_currentPosition的值，向后偏移数组大小
            m_currentPosition += totalSize;
        }

        return *this;
    }

    //如果没有申请到足够内存 抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

// @brief 该函数在给出不同字节顺序的情况下序列化一个int型数组(数组元素为4字节)
// @param long_t 将要序列化的int数组
// @param numElements int数组元素的数量
// @param endianness 序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serializeArray(const int32_t *long_t, size_t numElements, Endianness endianness)
{
    //临时变量，用于记录m_swapBytes
    bool auxSwap = m_swapBytes;
    //根据参数endianness以及机器字节存储顺序确定是否需要交换字节，即确定m_swapBytes的值
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {   
        //调用序列化函数
        serializeArray(long_t, numElements);
        //还原m_swapBytes
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //如果序列化失败则抛出异常并还原m_swapBytes
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

// @brief 该函数序列化一个wchar_t数组(双字节类型)
// @param wchar 将要序列化的w_char数组
// @param numElements 数组元素的数量
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serializeArray(const wchar_t *wchar, size_t numElements)
{
    //遍历wchar_t数组，调用序列化函数，分别序列化数组元素
    for(size_t count = 0; count < numElements; ++count)
        serialize(wchar[count]);
    return *this;
}

// @brief 该函数序列化一个wchar_t数组(双字节类型)
// @param wchar 将要序列化的w_char数组
// @param numElements 数组元素的数量
// @param endianness 序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serializeArray(const wchar_t *wchar, size_t numElements, Endianness endianness)
{
    //临时变量，用于记录m_swapBytes
    bool auxSwap = m_swapBytes;
    //根据参数endianness以及机器字节存储顺序确定是否需要交换字节，即确定m_swapBytes的值
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //根据最终确定的字节顺序调用序列化函数
        serializeArray(wchar, numElements);
        //还原m_swapBytes
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //如果序列化失败则抛出异常并还原m_swapBytes
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

// @brief 该函数序列化一个longlong数组
// @param longlong_t 将要序列化的longlong数组
// @param numElements 数组元素的数量
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serializeArray(const int64_t *longlong_t, size_t numElements)
{
    //需要对齐的字节数(仅与数组元素类型有关)
    size_t align = alignment(sizeof(*longlong_t));
    //数组总字节数
    size_t totalSize = sizeof(*longlong_t) * numElements;
    //序列化数据所需空间大小
    size_t sizeAligned = totalSize + align;

    //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        //存储最新数据的大小(数组元素)
        // Save last datasize.
        m_lastDataSize = sizeof(*longlong_t);

        //如果数组由元素则对齐数据
        // Align if there are any elements
        if(numElements)
            makeAlign(align);

        if(m_swapBytes)
        {
            //如果需要交换字节，把longlong数组转换为char数组在交换字节后序列化
            //dst为数组转换后的char指针
            const char *dst = reinterpret_cast<const char*>(&longlong_t);
            //end为数组最后一个元素的位置
            const char *end = dst + totalSize;

            //遍历数组元素，把longlong变量的每一个字节从后向前写入缓冲区，并同时更新m_currentPosition的值
            for(; dst < end; dst += sizeof(*longlong_t))
            {
                m_currentPosition++ << dst[7];
                m_currentPosition++ << dst[6];
                m_currentPosition++ << dst[5];
                m_currentPosition++ << dst[4];
                m_currentPosition++ << dst[3];
                m_currentPosition++ << dst[2];
                m_currentPosition++ << dst[1];
                m_currentPosition++ << dst[0];
            }
        }
        else
        {
            //如果不需要交换字节，直接把数组写入缓冲区，同时更新m_currentPosition的值
            m_currentPosition.memcopy(longlong_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    //如果没有申请到足够内存，抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}


// @brief 该函数在给出不同字节顺序的情况下序列化一个longlong型数组
// @param longlong_t 将要序列化的longlong数组
// @param numElements 数组元素的数量
// @param endianness 序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serializeArray(const int64_t *longlong_t, size_t numElements, Endianness endianness)
{
    //临时变量，用于记录m_swapBytes
    bool auxSwap = m_swapBytes;
    //根据参数endianness以及机器字节存储顺序确定是否需要交换字节，即确定m_swapBytes的值
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用序列化函数
        serializeArray(longlong_t, numElements);
        //还原m_swapBytes
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //如果序列化失败则抛出异常并还原m_swapBytes
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

// @brief 该函数序列化一个float数组
// @param float_t 将要序列化的float数组
// @param numElements 数组元素的数量
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serializeArray(const float *float_t, size_t numElements)
{
    //需要对齐的字节数(仅与数组元素类型有关)
    size_t align = alignment(sizeof(*float_t));
    //数组总字节数
    size_t totalSize = sizeof(*float_t) * numElements;
    //序列化数据所需空间大小
    size_t sizeAligned = totalSize + align;

    //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        //存储最新数据的大小(数组元素)
        // Save last datasize.
        m_lastDataSize = sizeof(*float_t);

        //如果数组有元素则对齐数据
        // Align if there are any elements
        if(numElements)
            makeAlign(align);

        if(m_swapBytes)
        {
            //如果需要交换字节，把float数组转换为char数组再交换字节后序列化
            //dst为数组转换后的char指针
            const char *dst = reinterpret_cast<const char*>(&float_t);
            //end为数组最后一个元素的位置
            const char *end = dst + totalSize;

            //遍历数组元素，把float的每一个字节从后向前写入缓冲区，并同时更新m_currentPosition的值
            for(; dst < end; dst += sizeof(*float_t))
            {
                m_currentPosition++ << dst[3];
                m_currentPosition++ << dst[2];
                m_currentPosition++ << dst[1];
                m_currentPosition++ << dst[0];
            }
        }
        else
        {
            //如果不需要交换字节，直接把数组写入缓冲区，同时更新m_currentPosition的值
            m_currentPosition.memcopy(float_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    //如果没有申请到足够内存，抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

// @brief 该函数在给出不同字节顺序的情况下序列化一个float型数组
// @param float_t 将要序列化的float数组
// @param numElements 数组元素的数量
// @param endianness 序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serializeArray(const float *float_t, size_t numElements, Endianness endianness)
{
    //临时变量，用于记录m_swapBytes
    bool auxSwap = m_swapBytes;
    //根据参数endianness以及机器字节存储顺序确定是否需要交换字节，即确定m_swapBytes的值
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用序列化函数，序列化成功后还原m_swapBytes
        serializeArray(float_t, numElements);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //如果序列化失败则抛出异常并还原m_swapBytes
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

// @brief 该函数序列化一个double数组
// @param double_t 将要序列化的double数组
// @param numElements 数组元素的数量
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serializeArray(const double *double_t, size_t numElements)
{
    //需要对齐的字节数(仅与数组元素类型有关)
    size_t align = alignment(sizeof(*double_t));
    //数组总字节数
    size_t totalSize = sizeof(*double_t) * numElements;
    //序列化数据所需空间大小
    size_t sizeAligned = totalSize + align;

    //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        //存储最新数据的大小(数组元素)
        // Save last datasize.
        m_lastDataSize = sizeof(*double_t);

        //如果数组有元素则对齐数据
        // Align if there are any elements
        if(numElements)
            makeAlign(align);

        if(m_swapBytes)
        {
            //如果需要交换字节，把double数组转换为char数组，交换字节后序列化
            //dst为数组转换后的char指针
            const char *dst = reinterpret_cast<const char*>(&double_t);
            //end为数组最后一个元素的位置
            const char *end = dst + totalSize;

            //遍历double_t的元素，把double变量的每一个字节从后向前写入缓冲区，并同时更新m_currentPosition的值
            for(; dst < end; dst += sizeof(*double_t))
            {
                m_currentPosition++ << dst[7];
                m_currentPosition++ << dst[6];
                m_currentPosition++ << dst[5];
                m_currentPosition++ << dst[4];
                m_currentPosition++ << dst[3];
                m_currentPosition++ << dst[2];
                m_currentPosition++ << dst[1];
                m_currentPosition++ << dst[0];
            }
        }
        else
        {
            //如果不需要交换字节，直接把数组写入缓冲区，同时更新m_currentPosition的值
            m_currentPosition.memcopy(double_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    //如果没有申请到足够内存，抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

// @brief 该函数在给出不同字节顺序的情况下序列化一个double型数组
// @param double_t 将要序列化的double数组
// @param numElements 数组元素的数量
// @param endianness 序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serializeArray(const double *double_t, size_t numElements, Endianness endianness)
{
    //临时变量，用于记录m_swapBytes
    bool auxSwap = m_swapBytes;
    //根据参数endianness以及机器字节存储顺序确定是否需要交换字节，即确定m_swapBytes的值
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用序列化函数，序列化成功后还原m_swapBytes
        serializeArray(double_t, numElements);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //如果序列化失败则抛出异常并还原m_swapBytes
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}
// linann end
// @brief 该函数用于序列化long double型数组
// @param ldouble_t 将要序列化的long double型数组
// @param numElements 数组元素个数
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @excepetion 尝试序列化超过内部存储器大小的位置时引发此异常
Cdr& Cdr::serializeArray(const long double *ldouble_t, size_t numElements)
{
    //需要对齐的字节数(仅与数组元素有关)
    size_t align = alignment(ALIGNMENT_LONG_DOUBLE);
    //long double数组的字节总数
    size_t totalSize = sizeof(*ldouble_t) * numElements;
    //序列化该组数据所需空间大小，包括数据本身大小和对齐
    size_t sizeAligned = totalSize + align;

    //判断当前缓冲区大小是否大于序列化所需空间 或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        // Save last datasize.
        m_lastDataSize = sizeof(*ldouble_t);

        // Align if there are any elements
        if(numElements)
            makeAlign(align);
        //如果需要交换字节顺序，把long double数组转为char型再序列化
        if(m_swapBytes)
        {
            //dst为long double数组转换后的char指针
            const char *dst = reinterpret_cast<const char*>(&ldouble_t);
            //end为数组结束的位置
            const char *end = dst + totalSize;
            //遍历数组元素，把char指针每一个字节从后向前写入缓冲区，并同时更新m_currentPosition的值
            for(; dst < end; dst += sizeof(*ldouble_t))
            {
                m_currentPosition++ << dst[15];
                m_currentPosition++ << dst[14];
                m_currentPosition++ << dst[13];
                m_currentPosition++ << dst[12];
                m_currentPosition++ << dst[11];
                m_currentPosition++ << dst[10];
                m_currentPosition++ << dst[9];
                m_currentPosition++ << dst[8];
                m_currentPosition++ << dst[7];
                m_currentPosition++ << dst[6];
                m_currentPosition++ << dst[5];
                m_currentPosition++ << dst[4];
                m_currentPosition++ << dst[3];
                m_currentPosition++ << dst[2];
                m_currentPosition++ << dst[1];
                m_currentPosition++ << dst[0];
            }
        }
        else
        {
            //如果不需要交换字节，直接把数组写入缓冲区
            m_currentPosition.memcopy(ldouble_t, totalSize);
            //同时更新m_currentPosition的值，向后偏移数组大小。
            m_currentPosition += totalSize;
        }

        return *this;
    }
    //如果没有申请到足够内存 抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}
// @brief 该函数用于序列化不同字节顺序的long double数组
// @param short_t 将要序列化的long double数组
// @param numElements 数组元素个数
// @param endianness 序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception 尝试序列化超过内部存储器大小的位置时引发此异常
Cdr& Cdr::serializeArray(const long double *ldouble_t, size_t numElements, Endianness endianness)
{
    //临时变量 记录m_swapBytes, 意为是否需要交换字节。
    bool auxSwap = m_swapBytes;
    //根据字节顺序以及机器-字节存储顺序确定是否需要交换字节
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));
    
    try
    {
//-----------------------------NOTE----------------------------------------------------
        //调用已有序列化函数
        (serial)izeArray(ldouble_t, numElements);
        //还原m_swapBytes
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //如果序列化失败抛出异常并且还原m_swapBytes
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}
// @brief 根据缓冲区内容反序列化到一字符上
// @param char_t 用于接收缓冲区信息的字符
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception 内存不足时抛出的异常
Cdr& Cdr::deserialize(char &char_t)
{
    //缓冲区内容长度大于字符长度 进行反序列化
    if((m_lastPosition - m_currentPosition) >= sizeof(char_t))
    {
        // Save last datasize.
        m_lastDataSize = sizeof(char_t);
        //将m_currentPosition的值赋给字符，反序列化成功加一即加上字符长度。
        m_currentPosition++ >> char_t;
        return *this;
    }
    //如果没有足够内存 抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}
// @brief 根据缓冲区内容反序列化到一short型数据上
// @param short_t 用于接收缓冲区信息的short型变量
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception 内存不足时抛出的异常
Cdr& Cdr::deserialize(int16_t &short_t)
{
    //需要对齐的字节数(仅与元素有关)
    size_t align = alignment(sizeof(short_t));
    //序列数据所需空间大小，包括数据本身大小和对齐
    size_t sizeAligned = sizeof(short_t) + align;
    //缓冲区内容长度大于序列化所需的空间 即内容足够多 进行反序列化
    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(short_t);

        // Align
        makeAlign(align);
        if(m_swapBytes)
        {
            //如果序列化过程中进行了交换字节 则将short型变量转换成char数组
            char *dst = reinterpret_cast<char*>(&short_t);
            //从下往上读取缓冲区中的内容至char数组中
            m_currentPosition++ >> dst[1];
            m_currentPosition++ >> dst[0];
        }
        else
        {
            //未发生字节的交换 则直接将缓冲区当前位置的值赋给short型变量
            //同时m_currentPosition加上short型变量长度
            m_currentPosition >> short_t;
            m_currentPosition += sizeof(short_t);
        }

        return *this;
    }
    //如果没有足够内存 抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}
// @brief 该函数用于根据缓冲区内容以及字节顺序反序列化至一short型变量上
// @param short_t 用于接收缓冲区内容的short型变量
// @param endianness 序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception 反序列化失败时抛出的异常
Cdr& Cdr::deserialize(int16_t &short_t, Endianness endianness)
{
    //临时变量， 存储m_swapBytes的值
    bool auxSwap = m_swapBytes;
    //根据endianness以及机器字节存储顺序确定是否需要交换字节
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用已有的反序列化函数
        deserialize(short_t);
        //还原m_swapBytes
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //若反序列化失败抛出异常并且还原m_swapBytes
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}
// @brief 根据缓冲区内容反序列化到一int型数据上
// @param long_t 用于接收缓冲区信息的int型变量
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception 内存不足时抛出的异常
Cdr& Cdr::deserialize(int32_t &long_t)
{
    //需要对齐的字节数(仅与元素有关)
    size_t align = alignment(sizeof(long_t));
    //序列化该数据所需空间大小，包括数据本身大小和对齐
    size_t sizeAligned = sizeof(long_t) + align;
    //缓冲区内容长度大于序列化所需的空间 即内容足够多 进行反序列化
    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(long_t);

        // Align
        makeAlign(align);

        if(m_swapBytes)
        {
            //如果序列化过程中进行了交换字节 则将int型变量转换成char数组
            char *dst = reinterpret_cast<char*>(&long_t);
            //从下往上读取缓冲区中的内容至char数组中
            m_currentPosition++ >> dst[3];
            m_currentPosition++ >> dst[2];
            m_currentPosition++ >> dst[1];
            m_currentPosition++ >> dst[0];
        }
        else
        {
            //未发生字节的交换 则直接将缓冲区当前位置的值赋给int型变量
            //同时m_currentPosition加上int型变量长度
            m_currentPosition >> long_t;
            m_currentPosition += sizeof(long_t);
        }

        return *this;
    }
    //如果没有足够内存 抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}
// @brief 该函数用于根据缓冲区内容以及字节顺序反序列化至一int型变量上
// @param long_t 用于接收缓冲区内容的int型变量
// @param endianness 序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception 反序列化失败时抛出的异常
Cdr& Cdr::deserialize(int32_t &long_t, Endianness endianness)
{
    //临时变量， 存储m_swapBytes的值
    bool auxSwap = m_swapBytes;
    //根据endianness以及机器字节存储顺序确定是否需要交换字节
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用已有的反序列化函数
        deserialize(long_t);
        //还原m_swapBytes的值
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //若反序列化失败 抛出异常并还原m_swapBytes的值
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}
// @brief 根据缓冲区内容反序列化到一longlong型数据上
// @param longlong_t 用于接收缓冲区信息的longlong型变量
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception 内存不足时抛出的异常
Cdr& Cdr::deserialize(int64_t &longlong_t)
{
    //需要对齐的字节数(仅与元素有关)
    size_t align = alignment(sizeof(longlong_t));
    //序列化该数据所需空间大小，包括数据本身大小和对齐
    size_t sizeAligned = sizeof(longlong_t) + align;
    //缓冲区内容长度大于序列化所需的空间 即内容足够多 进行反序列化
    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(longlong_t);

        // Align.
        makeAlign(align);

        if(m_swapBytes)
        {
            //如果序列化过程中进行了交换字节 则将longlong型变量转换成char数组
            char *dst = reinterpret_cast<char*>(&longlong_t);
            //从下往上读取缓冲区中的内容至char数组中
            m_currentPosition++ >> dst[7];
            m_currentPosition++ >> dst[6];
            m_currentPosition++ >> dst[5];
            m_currentPosition++ >> dst[4];
            m_currentPosition++ >> dst[3];
            m_currentPosition++ >> dst[2];
            m_currentPosition++ >> dst[1];
            m_currentPosition++ >> dst[0];
        }
        else
        {
            //未发生字节的交换 则直接将缓冲区当前位置的值赋给longlong型变量
            //同时m_currentPosition加上longlong型变量长度
            m_currentPosition >> longlong_t;
            m_currentPosition += sizeof(longlong_t);
        }

        return *this;
    }
    //内存不足 抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}
// @brief 该函数用于根据缓冲区内容以及字节顺序反序列化至一longlong型变量上
// @param longlong_t 用于接收缓冲区内容的longlong型变量
// @param endianness 序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception 反序列化失败时抛出的异常
Cdr& Cdr::deserialize(int64_t &longlong_t, Endianness endianness)
{
    //临时变量， 存储m_swapBytes的值
    bool auxSwap = m_swapBytes;
    //根据endianness以及机器字节存储顺序确定是否需要交换字节
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用已有的反序列化函数
        deserialize(longlong_t);
        //还原m_swapBytes的值
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //若反序列化失败 抛出异常并还原M_swapBytes的值
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}
// @brief 根据缓冲区内容反序列化到一float型数据上
// @param float_t 用于接收缓冲区信息的float型变量
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception 内存不足时抛出的异常
Cdr& Cdr::deserialize(float &float_t)
{
    //需要对齐的字节数(仅与元素有关)
    size_t align = alignment(sizeof(float_t));
    //序列化该数据所需空间大小，包括数据本身大小和对齐
    size_t sizeAligned = sizeof(float_t) + align;
    //缓冲区内容长度大于序列化所需的空间 即内容足够多 进行反序列化
    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(float_t);

        // Align.
        makeAlign(align);

        if(m_swapBytes)
        {
            //如果序列化过程中进行了交换字节 则将float型变量转换成char数组
            char *dst = reinterpret_cast<char*>(&float_t);
            //从下往上读取缓冲区中的内容至char数组中
            m_currentPosition++ >> dst[3];
            m_currentPosition++ >> dst[2];
            m_currentPosition++ >> dst[1];
            m_currentPosition++ >> dst[0];
        }
        else
        {
            //未发生字节的交换 则直接将缓冲区当前位置的值赋给float型变量
            //同时m_currentPosition加上float型变量长度
            m_currentPosition >> float_t;
            m_currentPosition += sizeof(float_t);
        }

        return *this;
    }
    //内存不足 抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}
// @brief 该函数用于根据缓冲区内容以及字节顺序反序列化至一float型变量上
// @param float_t 用于接收缓冲区内容的float型变量
// @param endianness 序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception 反序列化失败时抛出的异常
Cdr& Cdr::deserialize(float &float_t, Endianness endianness)
{
    //临时变量， 存储m_swapBytes的值
    bool auxSwap = m_swapBytes;
    //根据endianness以及机器字节存储顺序确定是否需要交换字节
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用已有的反序列化函数
        deserialize(float_t);
        //还原m_swapBytes的值
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //若反序列化失败 抛出异常并还原M_swapBytes的值
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}
// @brief 根据缓冲区内容反序列化到一double型数据上
// @param double_t 用于接收缓冲区信息的double型变量
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception 内存不足时抛出的异常
Cdr& Cdr::deserialize(double &double_t)
{
    //需要对齐的字节数(仅与元素有关)
    size_t align = alignment(sizeof(double_t));
    //序列化该数据所需空间大小，包括数据本身大小和对齐
    size_t sizeAligned = sizeof(double_t) + align;
    //缓冲区内容长度大于序列化所需的空间 即内容足够多 进行反序列化
    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(double_t);

        // Align.
        makeAlign(align);

        if(m_swapBytes)
        {
            //如果序列化过程中进行了交换字节 则将double型变量转换成char数组
            char *dst = reinterpret_cast<char*>(&double_t);
            //从下往上读取缓冲区中的内容至char数组中
            m_currentPosition++ >> dst[7];
            m_currentPosition++ >> dst[6];
            m_currentPosition++ >> dst[5];
            m_currentPosition++ >> dst[4];
            m_currentPosition++ >> dst[3];
            m_currentPosition++ >> dst[2];
            m_currentPosition++ >> dst[1];
            m_currentPosition++ >> dst[0];
        }
        else
        {
            //未发生字节的交换 则直接将缓冲区当前位置的值赋给double型变量
            //同时m_currentPosition加上double型变量长度
            m_currentPosition >> double_t;
            m_currentPosition += sizeof(double_t);
        }

        return *this;
    }
    //内存不足 抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}
// @brief 该函数用于根据缓冲区内容以及字节顺序反序列化至一double型变量上
// @param double_t 用于接收缓冲区内容的double型变量
// @param endianness 序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception 反序列化失败时抛出的异常
Cdr& Cdr::deserialize(double &double_t, Endianness endianness)
{
    //临时变量， 存储m_swapBytes的值
    bool auxSwap = m_swapBytes;
    //根据endianness以及机器字节存储顺序确定是否需要交换字节
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用已有的反序列化函数
        deserialize(double_t);
        //还原m_swapBytes的值
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //若反序列化失败 抛出异常并还原M_swapBytes的值
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

// @brief 根据缓冲区内容反序列化到一longdouble型数据上
// @param longdouble_t 用于接收缓冲区信息的longdouble型变量
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception 内存不足时抛出的异常
Cdr& Cdr::deserialize(long double &ldouble_t)
{
    //需要对齐的字节数(仅与元素有关)
    size_t align = alignment(ALIGNMENT_LONG_DOUBLE);
    //序列化该数据所需空间大小，包括数据本身大小和对齐
    size_t sizeAligned = sizeof(ldouble_t) + align;
    //缓冲区内容长度大于序列化所需的空间 即内容足够多 进行反序列化
    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(ldouble_t);

        // Align.
        makeAlign(align);

        if(m_swapBytes)
        {
            //如果序列化过程中进行了交换字节 则将double型变量转换成char数组
            char *dst = reinterpret_cast<char*>(&ldouble_t);
            //从下往上读取缓冲区中的内容至char数组中
            m_currentPosition++ >> dst[7];
            m_currentPosition++ >> dst[6];
            m_currentPosition++ >> dst[5];
            m_currentPosition++ >> dst[4];
            m_currentPosition++ >> dst[3];
            m_currentPosition++ >> dst[2];
            m_currentPosition++ >> dst[1];
            m_currentPosition++ >> dst[0];
        }
        else
        {
            //未发生字节的交换 则直接将缓冲区当前位置的值赋给double型变量
            //同时m_currentPosition加上double型变量长度
            m_currentPosition >> ldouble_t;
            m_currentPosition += sizeof(ldouble_t);
        }

        return *this;
    }
    //内存不足 抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}
// @brief 该函数用于根据缓冲区内容以及字节顺序反序列化至一longdouble型变量上
// @param longdouble_t 用于接收缓冲区内容的longdouble型变量
// @param endianness 序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception 反序列化失败时抛出的异常
Cdr& Cdr::deserialize(long double &ldouble_t, Endianness endianness)
{
    //临时变量， 存储m_swapBytes的值
    bool auxSwap = m_swapBytes;
    //根据endianness以及机器字节存储顺序确定是否需要交换字节
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用已有的反序列化函数
        deserialize(ldouble_t);
        //还原m_swapBytes的值
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //若反序列化失败 抛出异常并还原M_swapBytes的值
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

// @brief 根据缓冲区内容反序列化到一bool型数据上
// @param bool_t 用于接收缓冲区信息的bool型变量
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception 参数错误时抛出的异常
// @exception 内存不足时抛出的异常
Cdr& Cdr::deserialize(bool &bool_t)
{
    //临时变量 存储从缓冲区里读取的数据
    uint8_t value = 0;
    //缓冲区内容长度大于序列化所需的空间 即内容足够多 进行反序列化
    if((m_lastPosition - m_currentPosition) >= sizeof(uint8_t))
    {
        // Save last datasize.
        m_lastDataSize = sizeof(uint8_t);
        //缓冲区当前位置的值赋给uint8_t型变量
        m_currentPosition++ >> value;
        //根据value中读出来的值给bool型数据bool_t赋值
        if(value == 1)
        {
            bool_t = true;
            return *this;
        }
        else if(value == 0)
        {
            bool_t = false;
            return *this;
        }
        //value值不符合规范时 抛出异常
        throw BadParamException("Unexpected byte value in Cdr::deserialize(bool), expected 0 or 1");
    }
    //内存不足时 抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}
// @brief 该函数用于根据缓冲区内容以及字节顺序反序列化至一字符串型变量上
// @param string_t 用于接收缓冲区内容的字符串型变量
// @param endianness 序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception 反序列化失败时抛出的异常
Cdr& Cdr::deserialize(char *&string_t)
{
    //临时变量， 存储字符串长度
    uint32_t length = 0;
    //复制当前cdr序列化的状态
    Cdr::state state(*this);
    //将长度反序列化
    deserialize(length);

    if(length == 0)
    {
        //若长度为空，strng_t指向NUll，返回
        string_t = NULL;
        return *this;
    }
    else if((m_lastPosition - m_currentPosition) >= length)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(uint8_t);

        // Allocate memory.
        string_t = (char*)calloc(length + ((&m_currentPosition)[length-1] == '\0' ? 0 : 1), sizeof(char));
        //将缓冲区里的字符按长度输出到string_t指针指向的位置
        memcpy(string_t, &m_currentPosition, length);
        //更新m_currentPosition的值
        m_currentPosition += length;
        return *this;
    }
    //还原cdr序列化的状态
    setState(state);
    //内存不足 抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}
// @brief 该函数用于根据缓冲区内容以及字节顺序反序列化至一字符串型变量上
// @param string_t 用于接收缓冲区内容的字符串型变量
// @param endianness 序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception 反序列化失败时抛出的异常
Cdr& Cdr::deserialize(char *&string_t, Endianness endianness)
{
    //临时变量， 存储m_swapBytes的值
    bool auxSwap = m_swapBytes;
    //根据endianness以及机器字节存储顺序确定是否需要交换字节
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用已有的反序列化函数
        deserialize(string_t);
        //还原m_swapBytes的值
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //若反序列化失败 抛出异常并还原M_swapBytes的值
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}
// @brief 读取封装数据长度至一int型数据上
// @param length 存储字符串长度的int型变量
// @return 封装数据字符串的指针
const char* Cdr::readString(uint32_t &length)
{
    
    const char* returnedValue = "";
    //复制当前cdr序列化的状态
    state state(*this);
    //当前封装数据的长度赋给length
    *this >> length;

    if(length == 0)
    {
        //封装数据长度为0则直接返回""
        return returnedValue;
    }
    else if((m_lastPosition - m_currentPosition) >= length)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(uint8_t);
        //将缓冲区当前内容的地址指针赋给returnedValue
        returnedValue = &m_currentPosition;
        //更新m_currentPosition的值
        m_currentPosition += length;
        //如果封装数据末尾为'\0'结束符， 长度减1
        if(returnedValue[length-1] == '\0') --length;
        return returnedValue;
    }
    //还原cdr序列化的状态
    setState(state);
    //内存不足 抛出异常
    throw eprosima::fastcdr::exception::NotEnoughMemoryException(eprosima::fastcdr::exception::NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}
//------------------------------------ywj end---------------------------------------------------


/*!
 * @brief 这个函数反序列化一个布尔类型的数组
 * @param bool_t 用来存放这些布尔值的数组
 * @param numElements 数组中元素的个数
 * @return 返回这个Cdr对象的引用
 * @exception exception::NotEnoughMemoryException 如果目标数组处内存不够将抛出这个异常
 */
Cdr& Cdr::deserializeArray(bool *bool_t, size_t numElements)
{
    size_t totalSize = sizeof(*bool_t)*numElements; // 反序列化数据的长度

    if((m_lastPosition - m_currentPosition) >= totalSize)
    {
        // 如果空间足够
        // 保存最后一次反序列化的数据大小
        // Save last datasize.
        m_lastDataSize = sizeof(*bool_t);

        // 反序列化数组里的每个值
        for(size_t count = 0; count < numElements; ++count)
        {
            uint8_t value = 0;
            m_currentPosition++ >> value;

            if(value == 1)
            {
                bool_t[count] = true;
            }
            else if(value == 0)
            {
                bool_t[count] = false;
            }
        }

        return *this;
    }

    // 空间不足抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

/*!
 * @brief 这个函数反序列化一个char类型的数组
 * @param char_t 用来存放这些char值的数组
 * @param numElements 数组中元素的个数
 * @return 返回这个Cdr对象的引用
 * @exception exception::NotEnoughMemoryException 如果目标数组处内存不够将抛出这个异常
 */
Cdr& Cdr::deserializeArray(char *char_t, size_t numElements)
{
    size_t totalSize = sizeof(*char_t)*numElements; // 设置反序列化的数据长度

    if((m_lastPosition - m_currentPosition) >= totalSize)
    {
        // 如果空间足够
        // 保存最后一次反序列化的数据大小
        // Save last datasize.
        m_lastDataSize = sizeof(*char_t);

        // 反序列化
        m_currentPosition.rmemcopy(char_t, totalSize);
        m_currentPosition += totalSize;
        return *this;
    }

    // 如果空间不够, 抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

/*!
 * @brief 这个函数反序列化一个int16_t类型的数组
 * @param short_t 用来存放这些int16_t值的数组
 * @param numElements 数组中元素的个数
 * @return 返回这个Cdr对象的引用
 * @exception exception::NotEnoughMemoryException 如果目标数组处内存不够将抛出这个异常
 */
Cdr& Cdr::deserializeArray(int16_t *short_t, size_t numElements)
{
    size_t align = alignment(sizeof(*short_t));  // 设置对齐格式
    size_t totalSize = sizeof(*short_t) * numElements;  // 设置对齐所需空间
    size_t sizeAligned = totalSize + align;  // 设置对齐后的反序列化数据长度

    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // 如果空间足够
        // 保存这次所反序列化的数据长度
        m_lastDataSize = sizeof(*short_t);

        // Align if there are any elements
        if(numElements)
            makeAlign(align);
        // 设置对齐后开始反序列化

        if(m_swapBytes)
        {
            // 如果字节序是反的
            char *dst = reinterpret_cast<char*>(&short_t);
            char *end = dst + totalSize;

            // 反序列化数组里的每个值
            for(; dst < end; dst += sizeof(*short_t))
            {
                m_currentPosition++ >> dst[1];
                m_currentPosition++ >> dst[0];
            }
        }
        else
        {
            // 否则, 即字节序不是反的
            // 直接内存拷贝
            m_currentPosition.rmemcopy(short_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

/*!
 * @brief 这个函数反序列化一个int16_t类型的数组
 * @param short_t 用来存放这些int16_t值的数组
 * @param numElements 数组中元素的个数
 * @param endianness 这次反序列化将用到的字节序
 * @return 返回这个Cdr对象的引用
 * @exception exception::NotEnoughMemoryException 如果目标数组处内存不够将抛出这个异常
 */
Cdr& Cdr::deserializeArray(int16_t *short_t, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes; // 保存之前的字节序
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));
    // 设置反序列化的字节序

    try
    {
        // 反序列化数组
        deserializeArray(short_t, numElements);
        // 还原之前的字节序
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        // 抛出异常
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

/*!
 * @brief 这个函数反序列化一个int32_t类型的数组
 * @param long_t 用来存放这些int32_t值的数组
 * @param numElements 数组中元素的个数
 * @return 返回这个Cdr对象的引用
 * @exception exception::NotEnoughMemoryException 如果目标数组处内存不够将抛出这个异常
 */
Cdr& Cdr::deserializeArray(int32_t *long_t, size_t numElements)
{
    size_t align = alignment(sizeof(*long_t)); // 设置对齐格式
    size_t totalSize = sizeof(*long_t) * numElements; // 设置对齐所需空间
    size_t sizeAligned = totalSize + align; // 设置对齐后的反序列化数据长度

    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // 如果空间足够
        // 保存这次所反序列化的数据长度
        // Save last datasize.
        m_lastDataSize = sizeof(*long_t);

        // Align if there are any elements
        if(numElements)
            makeAlign(align);
        // 设置对齐后开始反序列化

        if(m_swapBytes)
        {
            // 如果字节序是反的
            char *dst = reinterpret_cast<char*>(&long_t);
            char *end = dst + totalSize;

            // 反序列化数组里的每个值
            for(; dst < end; dst += sizeof(*long_t))
            {
                m_currentPosition++ >> dst[3];
                m_currentPosition++ >> dst[2];
                m_currentPosition++ >> dst[1];
                m_currentPosition++ >> dst[0];
            }
        }
        else
        {
            // 否则, 即字节序不是反的
            // 直接内存拷贝
            m_currentPosition.rmemcopy(long_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

/*!
 * @brief 这个函数反序列化一个int32_t类型的数组
 * @param long_t 用来存放这些int32_t值的数组
 * @param numElements 数组中元素的个数
 * @param endianness 这次反序列化将用到的字节序
 * @return 返回这个Cdr对象的引用
 */
Cdr& Cdr::deserializeArray(int32_t *long_t, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes; // 保存之前的字节序
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness)); 
    // 设置反序列化的字节序

    try
    {
        // 反序列化数组
        deserializeArray(long_t, numElements);
        // 还原之前的字节序
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        // 抛出异常
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

/*!
 * @brief 这个函数反序列化一个wchar_t类型的数组
 * @param wchar 用来存放这些wchar_t值的数组
 * @param numElements 数组中元素的个数
 * @return 返回这个Cdr对象的引用
 */
Cdr& Cdr::deserializeArray(wchar_t *wchar, size_t numElements)
{
    uint32_t value; // 临时变量
    for(size_t count = 0; count < numElements; ++count)
    {
        // 反序列化每个值
        deserialize(value);
        // 输出到数组
        wchar[count] = (wchar_t)value;
    }
    return *this;
}

/*!
 * @brief 这个函数反序列化一个wchar_t类型的数组
 * @param wchar 用来存放这些wchar_t值的数组
 * @param numElements 数组中元素的个数
 * @param endianness 这次反序列化将用到的字节序
 * @return 返回这个Cdr对象的引用
 */
Cdr& Cdr::deserializeArray(wchar_t *wchar, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes;  // 保存之前的字节序
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));
    // 设置这次反序列化所用的字节序

    try
    {
        // 反序列化数组
        deserializeArray(wchar, numElements);
        // 还原之前的字节序
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        // 抛出异常
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

/*!
 * @brief 这个函数反序列化一个int64_t类型的数组
 * @param longlong_t 用来存放这些int64_t值的数组
 * @param numElements 数组中元素的个数
 * @return 返回这个Cdr对象的引用
 * @exception exception::NotEnoughMemoryException 如果目标数组处内存不够将抛出这个异常
 */
Cdr& Cdr::deserializeArray(int64_t *longlong_t, size_t numElements)
{
    size_t align = alignment(sizeof(*longlong_t));  // 设置对齐所需空间
    size_t totalSize = sizeof(*longlong_t) * numElements;  // 设置反序列化的数据长度
    size_t sizeAligned = totalSize + align;  // 设置对齐后的反序列化数据长度

    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    // 如果空间足够
    {
        // Save last datasize.
        m_lastDataSize = sizeof(*longlong_t);
        // 保存当前反序列化的数据大小

        // Align if there are any elements
        if(numElements)
            makeAlign(align);
        // 对齐后开始反序列化

        if(m_swapBytes)
        {
            // 如果字节序是反的
            char *dst = reinterpret_cast<char*>(&longlong_t);
            char *end = dst + totalSize;

            for(; dst < end; dst += sizeof(*longlong_t))
            {
                // 反序列化数组里的每个值
                m_currentPosition++ >> dst[7];
                m_currentPosition++ >> dst[6];
                m_currentPosition++ >> dst[5];
                m_currentPosition++ >> dst[4];
                m_currentPosition++ >> dst[3];
                m_currentPosition++ >> dst[2];
                m_currentPosition++ >> dst[1];
                m_currentPosition++ >> dst[0];
            }
        }
        else
        {
            // 否则, 即字节序不是反的
            m_currentPosition.rmemcopy(longlong_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

/*!
 * @brief 这个函数反序列化一个int64_t类型的数组
 * @param longlong_t 用来存放这些int64_t值的数组
 * @param numElements 数组中元素的个数
 * @param endianness 这次反序列化将用到的字节序
 * @return 返回这个Cdr对象的引用
 * @exception exception::NotEnoughMemoryException 如果目标数组处内存不够将抛出这个异常
 */
Cdr& Cdr::deserializeArray(int64_t *longlong_t, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes;  // 保存原来的字节序
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));
    // 设置新的字节序

    try
    {
        // 开始反序列化
        deserializeArray(longlong_t, numElements);
        // 还原之前的字节序
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    // 抛出异常
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

/*!
 * @brief 这个函数反序列化一个float类型的数组
 * @param float_t 用来存放这些float_t值的数组
 * @param numElements 数组中元素的个数
 * @return 返回这个Cdr对象的引用
 * @exception exception::NotEnoughMemoryException 如果目标数组处内存不够将抛出这个异常
 */
Cdr& Cdr::deserializeArray(float *float_t, size_t numElements)
{
    size_t align = alignment(sizeof(*float_t)); // 设置对齐格式
    size_t totalSize = sizeof(*float_t) * numElements; // 设置对齐后的反序列化数据长度
    size_t sizeAligned = totalSize + align; // 设置反序列化的数据长度

    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // Save last datasize.
        // 保存当前反序列化的数据大小
        m_lastDataSize = sizeof(*float_t);

        // Align if there are any elements
        if(numElements)
            makeAlign(align);

        // 对齐后开始反序列化
        if(m_swapBytes)
        // 如果字节序是反的
        {
            char *dst = reinterpret_cast<char*>(&float_t);
            char *end = dst + totalSize;

            // 反序列化数组里的每个值
            for(; dst < end; dst += sizeof(*float_t))
            {
                m_currentPosition++ >> dst[3];
                m_currentPosition++ >> dst[2];
                m_currentPosition++ >> dst[1];
                m_currentPosition++ >> dst[0];
            }
        }
        else
        // 否则, 即字节序不是反的
        {
            m_currentPosition.rmemcopy(float_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

/*!
 * @brief 这个函数反序列化一个float类型的数组
 * @param float_t 用来存放这些float_t值的数组
 * @param numElements 数组中元素的个数
 * @param endianness 这次反序列化将用到的字节序
 * @return 返回这个Cdr对象的引用
 * @exception exception::NotEnoughMemoryException 如果目标数组处内存不够将抛出这个异常
 */
Cdr& Cdr::deserializeArray(float *float_t, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes; // 保存原来的字节序
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness)); // 设置当前字节序

    try
    {
        // 反序列化数组
        deserializeArray(float_t, numElements);
        // 恢复原来的字节序
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    // 抛出异常
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

/*!
 * @brief 这个函数反序列化一个double类型的数组
 * @param double_t 用来存放这些double值的数组
 * @param numElements 数组中元素的个数
 * @return 返回这个Cdr对象的引用
 * @exception exception::NotEnoughMemoryException 如果目标数组处内存不够将抛出这个异常
 */
Cdr& Cdr::deserializeArray(double *double_t, size_t numElements)
{
    size_t align = alignment(sizeof(*double_t)); // 设置对齐格式
    size_t totalSize = sizeof(*double_t) * numElements; // 设置反序列化的数据长度
    size_t sizeAligned = totalSize + align; // 设置对齐后的反序列化数据长度

    if((m_lastPosition - m_currentPosition) >= sizeAligned) // 如果空间足够
    {
        // Save last datasize.
        // 保存当前反序列化的数据大小
        m_lastDataSize = sizeof(*double_t);

        // Align if there are any elements
        if(numElements)
            makeAlign(align);

        // 对齐, 开始反序列化
        if(m_swapBytes)
        // 如果字节序是反的
        {
            char *dst = reinterpret_cast<char*>(&double_t);
            char *end = dst + totalSize;
            
            // 反序列化数组里的每个值
            for(; dst < end; dst += sizeof(*double_t))
            {
                m_currentPosition++ >> dst[7];
                m_currentPosition++ >> dst[6];
                m_currentPosition++ >> dst[5];
                m_currentPosition++ >> dst[4];
                m_currentPosition++ >> dst[3];
                m_currentPosition++ >> dst[2];
                m_currentPosition++ >> dst[1];
                m_currentPosition++ >> dst[0];
            }
        }
        else
        // 否则, 即字节序不是反的
        {
            // 直接内存拷贝
            m_currentPosition.rmemcopy(double_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

/*!
 * @brief 这个函数反序列化一个double类型的数组
 * @param double_t 用来存放这些double值的数组
 * @param numElements 数组中元素的个数
 * @param endianness 这次反序列化将用到的字节序
 * @return 返回这个Cdr对象的引用
 * @exception exception::NotEnoughMemoryException 如果目标数组处内存不够将抛出这个异常
 */
Cdr& Cdr::deserializeArray(double *double_t, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes; // 保存原来的字节序
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));
    // 设置当前字节序
    try
    {
        // 反序列化数组
        deserializeArray(double_t, numElements);
        m_swapBytes = auxSwap; // 还原字节序
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

/*!
 * @brief 这个函数反序列化一个long double类型的数组
 * @param ldouble_t 用来存放这些long double值的数组
 * @param numElements 数组中元素的个数
 * @return 返回这个Cdr对象的引用
 * @exception exception::NotEnoughMemoryException 如果目标数组处内存不够将抛出这个异常
 */
Cdr& Cdr::deserializeArray(long double *ldouble_t, size_t numElements)
{
    size_t align = alignment(ALIGNMENT_LONG_DOUBLE); // 设置对齐格式为long double
    size_t totalSize = sizeof(*ldouble_t) * numElements; // 设置反序列化的数据长度
    size_t sizeAligned = totalSize + align; // 对齐后的反序列化长度

    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    // 如果空间足够
    {
        // Save last datasize.
        m_lastDataSize = sizeof(*ldouble_t);
        // 保存long double的数据大小

        // Align if there are any elements
        if(numElements)
            makeAlign(align);
        // 先对齐, 然后开始反序列化

        if(m_swapBytes)
        // 如果字节序是反的
        {
            char *dst = reinterpret_cast<char*>(&ldouble_t);
            char *end = dst + totalSize;

            for(; dst < end; dst += sizeof(*ldouble_t))
            // 对每个数据反序列化的时候将他们反过来
            {
                m_currentPosition++ >> dst[7];
                m_currentPosition++ >> dst[6];
                m_currentPosition++ >> dst[5];
                m_currentPosition++ >> dst[4];
                m_currentPosition++ >> dst[3];
                m_currentPosition++ >> dst[2];
                m_currentPosition++ >> dst[1];
                m_currentPosition++ >> dst[0];
            }
        }
        else
        // 否则, 即字节序不是反的, 那么直接拷贝内存
        {
            m_currentPosition.rmemcopy(ldouble_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

/*!
 * @brief 这个函数反序列化一个long double类型的数组
 * @param ldouble_t 用来存放这些long double值的数组
 * @param numElements 数组中元素的个数
 * @param endianness 这次反序列化将用到的字节序
 * @return 返回这个Cdr对象的引用
 */
Cdr& Cdr::deserializeArray(long double *ldouble_t, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    // 临时保存原来的m_swapBytes
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));
    // 设置这次反序列化所用的字节序

    try
    {
        // 反序列化这个数组
        deserializeArray(ldouble_t, numElements);
        // 还原此前保存的m_swapBytes
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

/*!
 * @brief 这个函数序列化一个bool类型的向量
 * @param vector_t 存放这些bool值的数组
 * @return 返回这个Cdr对象的引用
 * @exception exception::NotEnoughMemoryException 如果目标数组处内存不够将抛出这个异常
 */
Cdr& Cdr::serializeBoolSequence(const std::vector<bool> &vector_t)
{
    // 保存当前状态
    state state(*this);
    // 先序列化数组长度
    *this << (int32_t)vector_t.size();
    // 设置序列化的数据长度
    size_t totalSize = vector_t.size()*sizeof(bool);

    if(((m_lastPosition - m_currentPosition) >= totalSize) || resize(totalSize))
    // 如果空间足够
    {
        // Save last datasize.
        // 保存bool值的数据大小
        m_lastDataSize = sizeof(bool);
        // 序列化每一个布尔值
        for(size_t count = 0; count < vector_t.size(); ++count)
        {
            uint8_t value = 0;
            std::vector<bool>::const_reference ref = vector_t[count];

            if(ref)
                value = 1;
            m_currentPosition++ << value;
        }
    }
    else
    {
        setState(state);
        throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
    }

    return *this;
}

/*!
 * @brief 这个函数反序列化一个bool类型的数组
 * @param vector_t 用来存放这些double值的数组
 * @return 返回这个Cdr对象的引用
 * @exception exception::NotEnoughMemoryException 如果目标数组处内存不够将抛出这个异常
 * @exception exception::BadParamException 如果反序列化的目标不是bool类型
 */
Cdr& Cdr::deserializeBoolSequence(std::vector<bool> &vector_t)
{
    // 初始化长度为0
    uint32_t seqLength = 0;
    // 保存当前状态
    state state(*this);
    // 反序列化得到向量长度
    *this >> seqLength;

    // 设置vector长度和反序列化的数据长度
    vector_t.resize(seqLength);
    size_t totalSize = seqLength*sizeof(bool);

    if((m_lastPosition - m_currentPosition) >= totalSize)
    // 如果空间足够
    {
        // Save last datasize.
        // 保存bool值的数据大小
        m_lastDataSize = sizeof(bool);

        for(uint32_t count = 0; count < seqLength; ++count)
        {
            uint8_t value = 0;
            m_currentPosition++ >> value;
            // 反序列化出布尔值

            if(value == 1) 
            {
                vector_t[count] = true;
            }
            else if(value == 0)
            {
                vector_t[count] = false;
            } else {
                // 布尔值异常
                throw BadParamException("Unexpected byte value in Cdr::deserializeBoolSequence, expected 0 or 1");
            }
        }
    }
    else
    {
        setState(state);
        throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
    }

    return *this;
}

/*!
 * @brief 这个函数反序列化一个string类型的向量
 * @param sequence_t 用来存放这些字符串值的数组
 * @param numElements 数组中元素的个数
 * @return 返回这个Cdr对象的引用
 */
Cdr& Cdr::deserializeStringSequence(std::string *&sequence_t, size_t &numElements)
{
    // 初始化设置seqLenth为0
    uint32_t seqLength = 0;
    // 保存当前状态
    state state(*this);
    // 反序列化得到向量长度
    deserialize(seqLength);

    try
    {
        // 首先申请和分配空间
        sequence_t = (std::string*)calloc(seqLength, sizeof(std::string));
        for(uint32_t count = 0; count < seqLength; ++count)
            new(&sequence_t[count]) std::string;
        // 把每个字符串反序列化到相应位置
        deserializeArray(sequence_t, seqLength);
    }
    catch(eprosima::fastcdr::exception::Exception &ex)
    {
        // 如有异常首先释放空间
        free(sequence_t);
        // 再设置指针为空
        sequence_t = NULL;
        // 重新设置当前状态
        setState(state);
        // 异常处理
        ex.raise();
    }
    // 修改将要传出的值
    numElements = seqLength;
    return *this;
}
