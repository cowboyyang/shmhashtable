/* 一个共享内存上的HASHTABLE实现
 * @date 2014-06-19
 * @author cowboyyang
 */

#ifndef _MSHM_HASH_TABLE_H
#define _MSHM_HASH_TABLE_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <iostream>
#include <string.h> 
#include <hash_map>
#include <errno.h>
#include <stdio.h>

enum MHashCreateMode
{
    ENM_MHASH_SHM_UNKNOWN, // 未知状态
    ENM_MHASH_SHM_INIT,  // 初次创建
    ENM_MHASH_SHM_RESUME, // 恢复
};

enum MHashErrorCode
{
    ENM_MHASH_STATUS_OK = 0,
    ENM_MHASH_ERROR_GET_EXIST_SHM_FAIL = -1,
    ENM_MHASH_ERROR_GET_NEW_SHM_FAIL = -2,
    ENM_MHASH_ERROR_SHM_AT_FAIL = -3,
    ENM_MHASH_ERROR_CHECK_SHM_SIZE_FAIL = -4,
    ENM_MHASH_ERROR_CHECK_HASH_BUCKET_FAIL = -5,
    ENM_MHASH_ERROR_CHECK_DATA_NUM_FAIL = -6,
    ENM_MHASH_ERROR_DATA_NOT_EXIST = -7,
    ENM_MHASH_ERROR_DATA_ALLREADY_EXIST = -8,
    ENM_MHASH_ERROR_POOL_NO_FREE_NODE = -9,
    ENM_MHASH_ERROR_INVALID_SHM_KEY = -10,
    ENM_MHASH_ERROR_CALLBACK_FUNCTION_NULL = -11,
    ENM_MHASH_ERROR_NEED_DELETE_NODE = -12,
};

#define MHASH_SHOW_DATA_INFO printf

#define  MHASHADDR2OFFSET(addr)  ((char*)addr - (char*)m_pShmMemAddr)  // 地址转偏移
#define  MHASHOFFSET2ADDR(off)   ((char*)m_pShmMemAddr + off)  // 偏移转地址

// hash表头部定义
struct MTableHeadArea
{
    size_t MemSize;  // 共享内存大小
    // hash区域
    uint32_t dwHashBucketNum;  // hash bucket 数目
    // 节点统计信息
    uint32_t dwTotalDataNodeNum;  // 总数据节点数目
    uint32_t dwUsedDataNodeNum;  // 已经使用的数据节点数目
    uint32_t dwFreeDataHeadIndex;  //  空闲数据链表头索引
};

// hash node定义
struct MHashNode
{
    uint32_t dwNextNodeIdx;  // hash链中下一个节点索引
};

// 数据单元定义
template <typename TKey, typename TData>
struct MDataNode
{
    TKey key; // key定义
    TData data;  // 数据
                 // 此处可以定义canary, 用于数据校验
    uint32_t hashLinkNextIdx;  // hash链下个数据的索引
};


struct MHashConfig
{
    key_t shmKey;  // 共享内存key
    uint32_t dwMaxDataNodeNum;  // 最大数据个数
    uint32_t dwMaxHashBucketsNum;  // 最大的hash桶数目
};

// 默认使用__gnu_cxx中的hash函数，对于自定义数据，特化MHashFunction
template <typename TKey>
struct MHashFunction
{
public:
    size_t operator()(const TKey &key)
    {
        return m_hash_func_hasher(key);
    }

public:
    typedef __gnu_cxx::hash<TKey> MHashFunctionHasher;
private:
    MHashFunctionHasher m_hash_func_hasher;
};


template <typename TKey, typename TData, typename HashFn = MHashFunction<TKey> >
class MShmHashTable
{ 
public: 
    typedef HashFn Hasher;
public:
    MShmHashTable()
    {
        m_pShmMemAddr = NULL;
        m_pTableHeadAddr = NULL;
        m_pHashNodeHeadAddr = NULL;
        m_pDataNodeHeadAddr = NULL;
        enmCreateMode = ENM_MHASH_SHM_UNKNOWN;
    }

    /* 初始化创建共享hash表
     * @param stHashCfg hash表配置信息
     * @return ENM_MHASH_STATUS_OK表示创建成功，否则表示创建失败
     */
    MHashErrorCode InitHashTable(const MHashConfig &stHashCfg);

    /* 删除hash表中一个数据
     * @param key 数据键
     * @param 返回ENM_MHASH_STATUS_OK表示删除成功，否则表示删除失败
     */
    MHashErrorCode Erase(const TKey &key);

    /* 查找key值对应的数据
     * @param key 数据键值
     * @param iErrorCode 查找出错返回错误码
     * @return 获取成功返回对应的数据值，否则返回NULL
     */
    TData* Find(const TKey &key, MHashErrorCode &iErrorCode);

    /* 插入数据
     * @param key 数据键值
     * @param data 数据值
     * @return 返回ENM_MHASH_STATUS_OK表示插入成功，否则返回对应错误码
     */
    MHashErrorCode Insert(const TKey &key, const TData &data);

    /* 根据key获取一个空闲节点
     * @param key 数据键值
     * @param [out] retcode 错误码
     * @return 返还空闲节点地址，找不到返还NULL
     */
    TData* GetFreeNode(const TKey &key, MHashErrorCode &retcode);

    /* 处理表中每个节点
     * @param ProcessOneNode 回调处理函数
     * @param pData 回调数据
     * @return 
     */
    MHashErrorCode ProcessEveryNode( MHashErrorCode(*ProcessOneNode)(uint32_t, uint32_t, const TKey &, TData &, void *), void *pData);

    /* 打印运行数据
     */
    void PrintRunDataInfo();

    /* 获取错误对应的错误信息
     * @param errorCode 错误码
     * @return 返回错误码对应的错误信息
     */
    static const char* GetErrorString(MHashErrorCode errorCode);

    /* 获取启动模式
     * @return 返回启动模式
     */
    MHashCreateMode GetStartMode()
    {
        return enmCreateMode;
    }

private:
    /* 查找数据所在位置信息
     * @param key 数据key
     * @param dwPreIndex 数据在表中的前驱节点索引
     * @param dwIndex 数据在表中的索引
     * @return 返回ENM_MHASH_STATUS_OK表示获取到数据，否则表示获取数据失败
     */
    MHashErrorCode FindDataInfo(const TKey &key, uint32_t &dwPreIndex, uint32_t &dwIndex);


private:
    char *m_pShmMemAddr;  // 共享内存指针
    MTableHeadArea *m_pTableHeadAddr;  // 表头指针
    MHashNode *m_pHashNodeHeadAddr;  // hash区头指针
    MDataNode<TKey, TData> *m_pDataNodeHeadAddr;  // 数据区头指针
    MHashCreateMode enmCreateMode;  // 创建共享内存的模式
    MHashConfig m_stHashCfg;  // 创建时的配置信息

private:
    Hasher m_hasher_func;
};


/* 初始化创建共享hash表
 * @param stHashCfg hash表配置信息
 * @return 0表示创建成功，否则表示创建失败
 */
template <typename TKey, typename TData, typename HashFn>
MHashErrorCode MShmHashTable<TKey, TData, HashFn>::InitHashTable(const MHashConfig &stHashCfg)
{
    if (stHashCfg.shmKey <= 0)
    {
        // 非法共享内存key
        return ENM_MHASH_ERROR_INVALID_SHM_KEY;
    }

     memcpy(&m_stHashCfg, &stHashCfg, sizeof(m_stHashCfg));  // 保存配置信息

    size_t NeedMemSize = (sizeof(MTableHeadArea) + 
        sizeof(MHashNode) * stHashCfg.dwMaxHashBucketsNum + 
        sizeof(MDataNode<TKey, TData>) * stHashCfg.dwMaxDataNodeNum);

    int iShmID = shmget(stHashCfg.shmKey, NeedMemSize, IPC_CREAT | IPC_EXCL | 0666);
    if (-1 == iShmID)
    {
        if (EEXIST == errno)
        {
            iShmID = shmget(stHashCfg.shmKey, NeedMemSize, 0666);
            if (-1 == iShmID)
            {
                return ENM_MHASH_ERROR_GET_EXIST_SHM_FAIL;
            }

            enmCreateMode = ENM_MHASH_SHM_RESUME;  // 从已有共享内存上恢复
        }
        else
        {
            return ENM_MHASH_ERROR_GET_NEW_SHM_FAIL;
        }
    }
    else
    {
        enmCreateMode = ENM_MHASH_SHM_INIT;  // 首次创建共享内存
    }

    // 挂载共享内存
    m_pShmMemAddr = (char *)shmat(iShmID, NULL, 0);
    if (m_pShmMemAddr == (char*)-1)
    {
        return ENM_MHASH_ERROR_SHM_AT_FAIL;
    }

    m_pTableHeadAddr = (MTableHeadArea *)m_pShmMemAddr;  // 表头
    m_pHashNodeHeadAddr = (MHashNode *)((char*)m_pShmMemAddr + sizeof(MTableHeadArea));  // hash区域
    m_pDataNodeHeadAddr = (MDataNode<TKey, TData> * )((char*)m_pShmMemAddr + sizeof(MTableHeadArea) + 
        sizeof(MHashNode) * stHashCfg.dwMaxHashBucketsNum); // 数据区域

    // 首次创建，需要设置好hash头部信息
    if (ENM_MHASH_SHM_INIT == enmCreateMode )
    {
        memset(m_pShmMemAddr, 0, NeedMemSize);   
        m_pTableHeadAddr->MemSize = NeedMemSize;  // 共享内存大小
        m_pTableHeadAddr->dwTotalDataNodeNum = stHashCfg.dwMaxDataNodeNum;  // 数据总数
        m_pTableHeadAddr->dwUsedDataNodeNum = 0;  // 空闲数据节点数目
        m_pTableHeadAddr->dwHashBucketNum = stHashCfg.dwMaxHashBucketsNum;  // hash桶数目
        
        m_pTableHeadAddr->dwFreeDataHeadIndex = 1;  // 空闲链表指向第一个元素,0保留作判空

        // 将空闲节点连接起来
        for (uint32_t i = 0;  i<m_pTableHeadAddr->dwTotalDataNodeNum; ++i)
        {
            m_pDataNodeHeadAddr[i].hashLinkNextIdx = i+1;
        }
    }
    else
    {
        if (m_pTableHeadAddr->MemSize != NeedMemSize)
        {
            return ENM_MHASH_ERROR_CHECK_SHM_SIZE_FAIL;
        }
        else if (m_pTableHeadAddr->dwHashBucketNum != stHashCfg.dwMaxHashBucketsNum)
        {
            return ENM_MHASH_ERROR_CHECK_HASH_BUCKET_FAIL;
        }
        else if (m_pTableHeadAddr->dwTotalDataNodeNum != stHashCfg.dwMaxDataNodeNum)
        {
            return ENM_MHASH_ERROR_CHECK_DATA_NUM_FAIL;
        }
    }

    // NOTE: TData构造函数不应该做初始化处理，恢复共享内存时，会通过构造机制重新恢复虚函数指针
    // 初始化处理放于Init函数中，恢复处理放于Resume函数中

    // 对象初始处理
    if (ENM_MHASH_SHM_INIT == enmCreateMode)
    {
        // 初始创建
        for (uint32_t i = 0; i < m_pTableHeadAddr->dwTotalDataNodeNum; ++i)
        {
            // 出现异常，直接退出程序,TData构造函数应该为空，共享内存重启会重新调用构造函数，初始处理放在Init函数中
            TData *pData = new (&m_pDataNodeHeadAddr[i].data) TData();
            pData->Init();
        }
    }
    else if (ENM_MHASH_SHM_RESUME == enmCreateMode)
    {
        // 恢复处理
        for (uint32_t i = 0; i < m_pTableHeadAddr->dwTotalDataNodeNum; ++i)
        {
            // 出现异常，直接退出程序，TData构造函数应该为空，共享内存重启会重新调用构造函数，恢复处理放于Resume函数中
            TData *pData = new (&m_pDataNodeHeadAddr[i].data) TData();
            pData->Resume();
        }
    }

    return ENM_MHASH_STATUS_OK;
}

/* 查找数据所在位置信息
 * @param key 数据key
 * @param dwPreIndex 数据在表中的前驱节点索引
 * @param dwIndex 数据在表中的索引
 * @return 返回ENM_MHASH_STATUS_OK表示获取到数据，否则表示获取数据失败
 */
template <typename TKey, typename TData, typename HashFn>
MHashErrorCode MShmHashTable<TKey, TData, HashFn>::FindDataInfo(const TKey &key, uint32_t &dwPreIndex, uint32_t &dwIndex)
{
    size_t hash = m_hasher_func(key);
    uint32_t dwBucketIdx = hash % m_pTableHeadAddr->dwHashBucketNum;
    dwIndex = m_pHashNodeHeadAddr[dwBucketIdx].dwNextNodeIdx;
    dwPreIndex = dwIndex;

    while(0 != dwIndex)
    {
        if (key == m_pDataNodeHeadAddr[dwIndex].key)
        {
            return ENM_MHASH_STATUS_OK;
        }

        dwPreIndex = dwIndex;
        dwIndex = m_pDataNodeHeadAddr[dwIndex].hashLinkNextIdx;
    }

    return ENM_MHASH_ERROR_DATA_NOT_EXIST;
}


/* 查找key值对应的数据
 * @param key 数据键值
 * @param iErrorCode 查找出错返回错误码
 * @return 获取成功返回对应的数据值，否则返回NULL
 */
template <typename TKey, typename TData, typename HashFn>
TData* MShmHashTable<TKey, TData, HashFn>::Find(const TKey &key, MHashErrorCode &iErrorCode)
{
    uint32_t dwPreIndex = 0;
    uint32_t dwIndex = 0;
    iErrorCode = FindDataInfo(key, dwPreIndex, dwIndex);

    if (ENM_MHASH_STATUS_OK == iErrorCode)
    {
        return &m_pDataNodeHeadAddr[dwIndex].data;
    }
    
    return NULL;
}

/* 删除hash表中一个数据
 * @param key 数据键
 * @param 返回ENM_MHASH_STATUS_OK表示删除成功，否则表示删除失败
 */
template <typename TKey, typename TData, typename HashFn>
MHashErrorCode MShmHashTable<TKey, TData, HashFn>::Erase(const TKey &key)
{
    uint32_t dwPreIndex = 0;
    uint32_t dwIndex = 0;
    int iRet = FindDataInfo(key, dwPreIndex, dwIndex);

    if (ENM_MHASH_STATUS_OK == iRet)
    {
        if (dwIndex == dwPreIndex)
        {
            // 删除链上第一个元素
            size_t hash = m_hasher_func(key);
            uint32_t dwBucketIdx = hash % m_pTableHeadAddr->dwHashBucketNum;
            m_pHashNodeHeadAddr[dwBucketIdx].dwNextNodeIdx = m_pDataNodeHeadAddr[dwIndex].hashLinkNextIdx;
            MHASH_SHOW_DATA_INFO("bucketidx: %u, nextnodeidx: %u\n", 
                dwBucketIdx, m_pHashNodeHeadAddr[dwBucketIdx].dwNextNodeIdx);
        }
        else
        {
            m_pDataNodeHeadAddr[dwPreIndex].hashLinkNextIdx = m_pDataNodeHeadAddr[dwIndex].hashLinkNextIdx;
            MHASH_SHOW_DATA_INFO("none!\n");
        }

        m_pDataNodeHeadAddr[dwIndex].data.Reclaim();
            
        // 将删除的节点放入空闲链表中
        m_pDataNodeHeadAddr[dwIndex].hashLinkNextIdx = m_pTableHeadAddr->dwFreeDataHeadIndex;
        m_pTableHeadAddr->dwFreeDataHeadIndex = dwIndex;
        if (m_pTableHeadAddr->dwUsedDataNodeNum > 0)
        {
            m_pTableHeadAddr->dwUsedDataNodeNum--;
        }

        return ENM_MHASH_STATUS_OK;  // 删除成功
    }

    return ENM_MHASH_ERROR_DATA_NOT_EXIST;
}

/* 根据key获取一个空闲节点
 * @param key 数据键值
 * @param [out] retcode 错误码
 * @return 返还空闲节点地址，找不到返还NULL
 */
template <typename TKey, typename TData, typename HashFn>
TData* MShmHashTable<TKey, TData, HashFn>::GetFreeNode(const TKey &key, MHashErrorCode &retcode)
{
    // 0号节点没用，用于hash区判空
    if ( m_pTableHeadAddr->dwUsedDataNodeNum >= m_pTableHeadAddr->dwTotalDataNodeNum - 1)
    {
        retcode = ENM_MHASH_ERROR_POOL_NO_FREE_NODE;
        return NULL;  // 无空闲节点
    }

    // 判断数据是否已经存在
    uint32_t dwPreIndex = 0;
    uint32_t dwIndex = 0;
    int32_t iRet = FindDataInfo(key, dwPreIndex, dwIndex);
    if ( ENM_MHASH_STATUS_OK == iRet )
    {
        retcode = ENM_MHASH_ERROR_DATA_ALLREADY_EXIST;
        return NULL;  // key对应的数据已经存在
    }

    size_t hash = m_hasher_func(key);
    uint32_t dwBucketIdx = hash % m_pTableHeadAddr->dwHashBucketNum;
    uint32_t dwFreeNodeIdx = m_pTableHeadAddr->dwFreeDataHeadIndex;

    // 设置获取到的空闲节点key
    memcpy(&m_pDataNodeHeadAddr[dwFreeNodeIdx].key, &key, sizeof(TKey));

    // 初始化值节点
    m_pDataNodeHeadAddr[dwFreeNodeIdx].data.Init();

    m_pTableHeadAddr->dwFreeDataHeadIndex = m_pDataNodeHeadAddr[dwFreeNodeIdx].hashLinkNextIdx;
    m_pDataNodeHeadAddr[dwFreeNodeIdx].hashLinkNextIdx = m_pHashNodeHeadAddr[dwBucketIdx].dwNextNodeIdx;
    m_pHashNodeHeadAddr[dwBucketIdx].dwNextNodeIdx = dwFreeNodeIdx;
    m_pTableHeadAddr->dwUsedDataNodeNum++;

    return &m_pDataNodeHeadAddr[dwFreeNodeIdx].data;
}

/* 插入数据
 * @param key 数据键值
 * @param data 数据值
 * @return 返回ENM_MHASH_STATUS_OK表示插入成功，否则返回对应错误码
 */
template <typename TKey, typename TData, typename HashFn>
MHashErrorCode MShmHashTable<TKey, TData, HashFn>::Insert(const TKey &key, const TData &data)
{
    // 0号节点没用，用于hash区判空
    if (m_pTableHeadAddr->dwUsedDataNodeNum >= m_pTableHeadAddr->dwTotalDataNodeNum - 1)
    {
        return ENM_MHASH_ERROR_POOL_NO_FREE_NODE;  // 无空闲节点
    }

    uint32_t dwPreIndex = 0;
    uint32_t dwIndex = 0;
    int32_t iRet = FindDataInfo(key, dwPreIndex, dwIndex);
    if (ENM_MHASH_STATUS_OK == iRet)
    {
        return ENM_MHASH_ERROR_DATA_ALLREADY_EXIST;  // 数据已经存在
    }

    size_t hash = m_hasher_func(key);
    uint32_t dwBucketIdx = hash % m_pTableHeadAddr->dwHashBucketNum;
    uint32_t dwFreeNodeIdx = m_pTableHeadAddr->dwFreeDataHeadIndex;
    m_pTableHeadAddr->dwFreeDataHeadIndex = m_pDataNodeHeadAddr[dwFreeNodeIdx].hashLinkNextIdx;

    memcpy(&m_pDataNodeHeadAddr[dwFreeNodeIdx].key, &key, sizeof(TKey));
    memcpy(&m_pDataNodeHeadAddr[dwFreeNodeIdx].data, &data, sizeof(TData));
    m_pDataNodeHeadAddr[dwFreeNodeIdx].hashLinkNextIdx = m_pHashNodeHeadAddr[dwBucketIdx].dwNextNodeIdx;
    m_pHashNodeHeadAddr[dwBucketIdx].dwNextNodeIdx = dwFreeNodeIdx;
    m_pTableHeadAddr->dwUsedDataNodeNum++;

    return ENM_MHASH_STATUS_OK;
}

/* 处理表中每个节点
 * @param ProcessOneNode 回调处理函数
 * @param pData 回调数据
 * @return 
 */
template <typename TKey, typename TData, typename HashFn>
MHashErrorCode MShmHashTable<TKey, TData, HashFn>::ProcessEveryNode( MHashErrorCode(*ProcessOneNode)(uint32_t, uint32_t, const TKey &, TData &, void *), void *pData )
{
    if (NULL == ProcessOneNode)
    {
        MHASH_SHOW_DATA_INFO("ProcessOneNode is Null, Now return!\n");
        return ENM_MHASH_ERROR_CALLBACK_FUNCTION_NULL;
    }

    uint32_t dwDataIndex;
    uint32_t dwPreDataIndex;
    for (uint32_t i = 0; i < m_pTableHeadAddr->dwHashBucketNum; ++i)
    {
        dwDataIndex = m_pHashNodeHeadAddr[i].dwNextNodeIdx;
        dwPreDataIndex = dwDataIndex;
        while(dwDataIndex > 0)
        {
            MHashErrorCode errorCode = ProcessOneNode(i, dwDataIndex, m_pDataNodeHeadAddr[dwDataIndex].key, 
                m_pDataNodeHeadAddr[dwDataIndex].data, pData);
            if (ENM_MHASH_ERROR_NEED_DELETE_NODE == errorCode)
            {
                // 需要删除当前处理节点
                if (dwPreDataIndex == dwDataIndex)
                {
                    // 第一个节点
                    m_pHashNodeHeadAddr[i].dwNextNodeIdx = m_pDataNodeHeadAddr[dwDataIndex].hashLinkNextIdx;
                }
                else
                {
                    m_pDataNodeHeadAddr[dwPreDataIndex].hashLinkNextIdx = m_pDataNodeHeadAddr[dwDataIndex].hashLinkNextIdx;
                }

                uint32_t dwTmpDataIndex = m_pDataNodeHeadAddr[dwDataIndex].hashLinkNextIdx;
                // 将删除的节点放入空闲链中
                m_pDataNodeHeadAddr[dwDataIndex].data.Reclaim();
                m_pDataNodeHeadAddr[dwDataIndex].hashLinkNextIdx = m_pTableHeadAddr->dwFreeDataHeadIndex;
                m_pTableHeadAddr->dwFreeDataHeadIndex = dwDataIndex;
                if (m_pTableHeadAddr->dwUsedDataNodeNum > 0)
                {
                    m_pTableHeadAddr->dwUsedDataNodeNum--;
                }

                dwDataIndex = dwTmpDataIndex;
            }
            else
            {
                dwPreDataIndex = dwDataIndex;
                dwDataIndex = m_pDataNodeHeadAddr[dwDataIndex].hashLinkNextIdx;
            }
        }
    }

    return ENM_MHASH_STATUS_OK;
}


/* 打印运行数据
 */
template <typename TKey, typename TData, typename HashFn>
void MShmHashTable<TKey, TData, HashFn>::PrintRunDataInfo()
{
    MHASH_SHOW_DATA_INFO("shmkey(%d)\n", (int)m_stHashCfg.shmKey);
    MHASH_SHOW_DATA_INFO("totalDataNode(%d)\n", m_pTableHeadAddr->dwTotalDataNodeNum);
    MHASH_SHOW_DATA_INFO("usedDataNode(%d)\n", m_pTableHeadAddr->dwUsedDataNodeNum);
    MHASH_SHOW_DATA_INFO("bucketsNum(%d)\n", m_pTableHeadAddr->dwHashBucketNum);
    MHASH_SHOW_DATA_INFO("memsize(%lu)\n", m_pTableHeadAddr->MemSize);
    return;
}

/* 获取错误对应的错误信息
 * @param errorCode 错误码
 * @return 返回错误码对应的错误信息
 */
template <typename TKey, typename TData, typename HashFn>
const char* MShmHashTable<TKey, TData, HashFn>::GetErrorString(MHashErrorCode errorCode)
{
    // 错误信息表
    static const char* errorTable[] =
    {
        /* ENM_MHASH_STATUS_OK = 0 */ "no error",
        /* ENM_MHASH_ERROR_GET_EXIST_SHM_FAIL = -1 */ "get existshm fail",
        /* ENM_MHASH_ERROR_GET_NEW_SHM_FAIL = -2 */ "get newshm fail",
        /* ENM_MHASH_ERROR_SHM_AT_FAIL = -3 */ "shmat fail",
        /* ENM_MHASH_ERROR_CHECK_SHM_SIZE_FAIL = -4 */ "check shmsize fail",
        /* ENM_MHASH_ERROR_CHECK_HASH_BUCKET_FAIL = -5 */ "check hashbucket fail",
        /* ENM_MHASH_ERROR_CHECK_DATA_NUM_FAIL = -6 */ "check data num fail",
        /* ENM_MHASH_ERROR_DATA_NOT_EXIST = -7 */ "data not exist",
        /* ENM_MHASH_ERROR_DATA_ALLREADY_EXIST = -8 */ "data allready exist",
        /* ENM_MHASH_ERROR_POOL_NO_FREE_NODE = -9 */ "shm pool no free node",
        /* ENM_MHASH_ERROR_INVALID_SHM_KEY = -10 */ "invalid shm key",
        /* ENM_MHASH_ERROR_CALLBACK_FUNCTION_NULL = -11 */ "callback function null",
        /* ENM_MHASH_ERROR_NEED_DELETE_NODE = -12 */ "need delete node",
    };

    int iErrorIndex = -1 * errorCode;
    int iErrorNum = sizeof(errorTable) / sizeof(errorTable[0]);
    if (iErrorIndex >= 0 && iErrorIndex < iErrorNum)
    {
        return errorTable[iErrorIndex];
    }

    return "unknown error";
}

#endif
