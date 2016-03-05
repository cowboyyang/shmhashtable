/* һ�������ڴ��ϵ�HASHTABLEʵ��
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
    ENM_MHASH_SHM_UNKNOWN, // δ֪״̬
    ENM_MHASH_SHM_INIT,  // ���δ���
    ENM_MHASH_SHM_RESUME, // �ָ�
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

#define  MHASHADDR2OFFSET(addr)  ((char*)addr - (char*)m_pShmMemAddr)  // ��ַתƫ��
#define  MHASHOFFSET2ADDR(off)   ((char*)m_pShmMemAddr + off)  // ƫ��ת��ַ

// hash��ͷ������
struct MTableHeadArea
{
    size_t MemSize;  // �����ڴ��С
    // hash����
    uint32_t dwHashBucketNum;  // hash bucket ��Ŀ
    // �ڵ�ͳ����Ϣ
    uint32_t dwTotalDataNodeNum;  // �����ݽڵ���Ŀ
    uint32_t dwUsedDataNodeNum;  // �Ѿ�ʹ�õ����ݽڵ���Ŀ
    uint32_t dwFreeDataHeadIndex;  //  ������������ͷ����
};

// hash node����
struct MHashNode
{
    uint32_t dwNextNodeIdx;  // hash������һ���ڵ�����
};

// ���ݵ�Ԫ����
template <typename TKey, typename TData>
struct MDataNode
{
    TKey key; // key����
    TData data;  // ����
                 // �˴����Զ���canary, ��������У��
    uint32_t hashLinkNextIdx;  // hash���¸����ݵ�����
};


struct MHashConfig
{
    key_t shmKey;  // �����ڴ�key
    uint32_t dwMaxDataNodeNum;  // ������ݸ���
    uint32_t dwMaxHashBucketsNum;  // ����hashͰ��Ŀ
};

// Ĭ��ʹ��__gnu_cxx�е�hash�����������Զ������ݣ��ػ�MHashFunction
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

    /* ��ʼ����������hash��
     * @param stHashCfg hash��������Ϣ
     * @return ENM_MHASH_STATUS_OK��ʾ�����ɹ��������ʾ����ʧ��
     */
    MHashErrorCode InitHashTable(const MHashConfig &stHashCfg);

    /* ɾ��hash����һ������
     * @param key ���ݼ�
     * @param ����ENM_MHASH_STATUS_OK��ʾɾ���ɹ��������ʾɾ��ʧ��
     */
    MHashErrorCode Erase(const TKey &key);

    /* ����keyֵ��Ӧ������
     * @param key ���ݼ�ֵ
     * @param iErrorCode ���ҳ����ش�����
     * @return ��ȡ�ɹ����ض�Ӧ������ֵ�����򷵻�NULL
     */
    TData* Find(const TKey &key, MHashErrorCode &iErrorCode);

    /* ��������
     * @param key ���ݼ�ֵ
     * @param data ����ֵ
     * @return ����ENM_MHASH_STATUS_OK��ʾ����ɹ������򷵻ض�Ӧ������
     */
    MHashErrorCode Insert(const TKey &key, const TData &data);

    /* ����key��ȡһ�����нڵ�
     * @param key ���ݼ�ֵ
     * @param [out] retcode ������
     * @return �������нڵ��ַ���Ҳ�������NULL
     */
    TData* GetFreeNode(const TKey &key, MHashErrorCode &retcode);

    /* �������ÿ���ڵ�
     * @param ProcessOneNode �ص�������
     * @param pData �ص�����
     * @return 
     */
    MHashErrorCode ProcessEveryNode( MHashErrorCode(*ProcessOneNode)(uint32_t, uint32_t, const TKey &, TData &, void *), void *pData);

    /* ��ӡ��������
     */
    void PrintRunDataInfo();

    /* ��ȡ�����Ӧ�Ĵ�����Ϣ
     * @param errorCode ������
     * @return ���ش������Ӧ�Ĵ�����Ϣ
     */
    static const char* GetErrorString(MHashErrorCode errorCode);

    /* ��ȡ����ģʽ
     * @return ��������ģʽ
     */
    MHashCreateMode GetStartMode()
    {
        return enmCreateMode;
    }

private:
    /* ������������λ����Ϣ
     * @param key ����key
     * @param dwPreIndex �����ڱ��е�ǰ���ڵ�����
     * @param dwIndex �����ڱ��е�����
     * @return ����ENM_MHASH_STATUS_OK��ʾ��ȡ�����ݣ������ʾ��ȡ����ʧ��
     */
    MHashErrorCode FindDataInfo(const TKey &key, uint32_t &dwPreIndex, uint32_t &dwIndex);


private:
    char *m_pShmMemAddr;  // �����ڴ�ָ��
    MTableHeadArea *m_pTableHeadAddr;  // ��ͷָ��
    MHashNode *m_pHashNodeHeadAddr;  // hash��ͷָ��
    MDataNode<TKey, TData> *m_pDataNodeHeadAddr;  // ������ͷָ��
    MHashCreateMode enmCreateMode;  // ���������ڴ��ģʽ
    MHashConfig m_stHashCfg;  // ����ʱ��������Ϣ

private:
    Hasher m_hasher_func;
};


/* ��ʼ����������hash��
 * @param stHashCfg hash��������Ϣ
 * @return 0��ʾ�����ɹ��������ʾ����ʧ��
 */
template <typename TKey, typename TData, typename HashFn>
MHashErrorCode MShmHashTable<TKey, TData, HashFn>::InitHashTable(const MHashConfig &stHashCfg)
{
    if (stHashCfg.shmKey <= 0)
    {
        // �Ƿ������ڴ�key
        return ENM_MHASH_ERROR_INVALID_SHM_KEY;
    }

     memcpy(&m_stHashCfg, &stHashCfg, sizeof(m_stHashCfg));  // ����������Ϣ

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

            enmCreateMode = ENM_MHASH_SHM_RESUME;  // �����й����ڴ��ϻָ�
        }
        else
        {
            return ENM_MHASH_ERROR_GET_NEW_SHM_FAIL;
        }
    }
    else
    {
        enmCreateMode = ENM_MHASH_SHM_INIT;  // �״δ��������ڴ�
    }

    // ���ع����ڴ�
    m_pShmMemAddr = (char *)shmat(iShmID, NULL, 0);
    if (m_pShmMemAddr == (char*)-1)
    {
        return ENM_MHASH_ERROR_SHM_AT_FAIL;
    }

    m_pTableHeadAddr = (MTableHeadArea *)m_pShmMemAddr;  // ��ͷ
    m_pHashNodeHeadAddr = (MHashNode *)((char*)m_pShmMemAddr + sizeof(MTableHeadArea));  // hash����
    m_pDataNodeHeadAddr = (MDataNode<TKey, TData> * )((char*)m_pShmMemAddr + sizeof(MTableHeadArea) + 
        sizeof(MHashNode) * stHashCfg.dwMaxHashBucketsNum); // ��������

    // �״δ�������Ҫ���ú�hashͷ����Ϣ
    if (ENM_MHASH_SHM_INIT == enmCreateMode )
    {
        memset(m_pShmMemAddr, 0, NeedMemSize);   
        m_pTableHeadAddr->MemSize = NeedMemSize;  // �����ڴ��С
        m_pTableHeadAddr->dwTotalDataNodeNum = stHashCfg.dwMaxDataNodeNum;  // ��������
        m_pTableHeadAddr->dwUsedDataNodeNum = 0;  // �������ݽڵ���Ŀ
        m_pTableHeadAddr->dwHashBucketNum = stHashCfg.dwMaxHashBucketsNum;  // hashͰ��Ŀ
        
        m_pTableHeadAddr->dwFreeDataHeadIndex = 1;  // ��������ָ���һ��Ԫ��,0�������п�

        // �����нڵ���������
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

    // NOTE: TData���캯����Ӧ������ʼ�������ָ������ڴ�ʱ����ͨ������������»ָ��麯��ָ��
    // ��ʼ���������Init�����У��ָ��������Resume������

    // �����ʼ����
    if (ENM_MHASH_SHM_INIT == enmCreateMode)
    {
        // ��ʼ����
        for (uint32_t i = 0; i < m_pTableHeadAddr->dwTotalDataNodeNum; ++i)
        {
            // �����쳣��ֱ���˳�����,TData���캯��Ӧ��Ϊ�գ������ڴ����������µ��ù��캯������ʼ�������Init������
            TData *pData = new (&m_pDataNodeHeadAddr[i].data) TData();
            pData->Init();
        }
    }
    else if (ENM_MHASH_SHM_RESUME == enmCreateMode)
    {
        // �ָ�����
        for (uint32_t i = 0; i < m_pTableHeadAddr->dwTotalDataNodeNum; ++i)
        {
            // �����쳣��ֱ���˳�����TData���캯��Ӧ��Ϊ�գ������ڴ����������µ��ù��캯�����ָ��������Resume������
            TData *pData = new (&m_pDataNodeHeadAddr[i].data) TData();
            pData->Resume();
        }
    }

    return ENM_MHASH_STATUS_OK;
}

/* ������������λ����Ϣ
 * @param key ����key
 * @param dwPreIndex �����ڱ��е�ǰ���ڵ�����
 * @param dwIndex �����ڱ��е�����
 * @return ����ENM_MHASH_STATUS_OK��ʾ��ȡ�����ݣ������ʾ��ȡ����ʧ��
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


/* ����keyֵ��Ӧ������
 * @param key ���ݼ�ֵ
 * @param iErrorCode ���ҳ����ش�����
 * @return ��ȡ�ɹ����ض�Ӧ������ֵ�����򷵻�NULL
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

/* ɾ��hash����һ������
 * @param key ���ݼ�
 * @param ����ENM_MHASH_STATUS_OK��ʾɾ���ɹ��������ʾɾ��ʧ��
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
            // ɾ�����ϵ�һ��Ԫ��
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
            
        // ��ɾ���Ľڵ�������������
        m_pDataNodeHeadAddr[dwIndex].hashLinkNextIdx = m_pTableHeadAddr->dwFreeDataHeadIndex;
        m_pTableHeadAddr->dwFreeDataHeadIndex = dwIndex;
        if (m_pTableHeadAddr->dwUsedDataNodeNum > 0)
        {
            m_pTableHeadAddr->dwUsedDataNodeNum--;
        }

        return ENM_MHASH_STATUS_OK;  // ɾ���ɹ�
    }

    return ENM_MHASH_ERROR_DATA_NOT_EXIST;
}

/* ����key��ȡһ�����нڵ�
 * @param key ���ݼ�ֵ
 * @param [out] retcode ������
 * @return �������нڵ��ַ���Ҳ�������NULL
 */
template <typename TKey, typename TData, typename HashFn>
TData* MShmHashTable<TKey, TData, HashFn>::GetFreeNode(const TKey &key, MHashErrorCode &retcode)
{
    // 0�Žڵ�û�ã�����hash���п�
    if ( m_pTableHeadAddr->dwUsedDataNodeNum >= m_pTableHeadAddr->dwTotalDataNodeNum - 1)
    {
        retcode = ENM_MHASH_ERROR_POOL_NO_FREE_NODE;
        return NULL;  // �޿��нڵ�
    }

    // �ж������Ƿ��Ѿ�����
    uint32_t dwPreIndex = 0;
    uint32_t dwIndex = 0;
    int32_t iRet = FindDataInfo(key, dwPreIndex, dwIndex);
    if ( ENM_MHASH_STATUS_OK == iRet )
    {
        retcode = ENM_MHASH_ERROR_DATA_ALLREADY_EXIST;
        return NULL;  // key��Ӧ�������Ѿ�����
    }

    size_t hash = m_hasher_func(key);
    uint32_t dwBucketIdx = hash % m_pTableHeadAddr->dwHashBucketNum;
    uint32_t dwFreeNodeIdx = m_pTableHeadAddr->dwFreeDataHeadIndex;

    // ���û�ȡ���Ŀ��нڵ�key
    memcpy(&m_pDataNodeHeadAddr[dwFreeNodeIdx].key, &key, sizeof(TKey));

    // ��ʼ��ֵ�ڵ�
    m_pDataNodeHeadAddr[dwFreeNodeIdx].data.Init();

    m_pTableHeadAddr->dwFreeDataHeadIndex = m_pDataNodeHeadAddr[dwFreeNodeIdx].hashLinkNextIdx;
    m_pDataNodeHeadAddr[dwFreeNodeIdx].hashLinkNextIdx = m_pHashNodeHeadAddr[dwBucketIdx].dwNextNodeIdx;
    m_pHashNodeHeadAddr[dwBucketIdx].dwNextNodeIdx = dwFreeNodeIdx;
    m_pTableHeadAddr->dwUsedDataNodeNum++;

    return &m_pDataNodeHeadAddr[dwFreeNodeIdx].data;
}

/* ��������
 * @param key ���ݼ�ֵ
 * @param data ����ֵ
 * @return ����ENM_MHASH_STATUS_OK��ʾ����ɹ������򷵻ض�Ӧ������
 */
template <typename TKey, typename TData, typename HashFn>
MHashErrorCode MShmHashTable<TKey, TData, HashFn>::Insert(const TKey &key, const TData &data)
{
    // 0�Žڵ�û�ã�����hash���п�
    if (m_pTableHeadAddr->dwUsedDataNodeNum >= m_pTableHeadAddr->dwTotalDataNodeNum - 1)
    {
        return ENM_MHASH_ERROR_POOL_NO_FREE_NODE;  // �޿��нڵ�
    }

    uint32_t dwPreIndex = 0;
    uint32_t dwIndex = 0;
    int32_t iRet = FindDataInfo(key, dwPreIndex, dwIndex);
    if (ENM_MHASH_STATUS_OK == iRet)
    {
        return ENM_MHASH_ERROR_DATA_ALLREADY_EXIST;  // �����Ѿ�����
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

/* �������ÿ���ڵ�
 * @param ProcessOneNode �ص�������
 * @param pData �ص�����
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
                // ��Ҫɾ����ǰ����ڵ�
                if (dwPreDataIndex == dwDataIndex)
                {
                    // ��һ���ڵ�
                    m_pHashNodeHeadAddr[i].dwNextNodeIdx = m_pDataNodeHeadAddr[dwDataIndex].hashLinkNextIdx;
                }
                else
                {
                    m_pDataNodeHeadAddr[dwPreDataIndex].hashLinkNextIdx = m_pDataNodeHeadAddr[dwDataIndex].hashLinkNextIdx;
                }

                uint32_t dwTmpDataIndex = m_pDataNodeHeadAddr[dwDataIndex].hashLinkNextIdx;
                // ��ɾ���Ľڵ�����������
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


/* ��ӡ��������
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

/* ��ȡ�����Ӧ�Ĵ�����Ϣ
 * @param errorCode ������
 * @return ���ش������Ӧ�Ĵ�����Ϣ
 */
template <typename TKey, typename TData, typename HashFn>
const char* MShmHashTable<TKey, TData, HashFn>::GetErrorString(MHashErrorCode errorCode)
{
    // ������Ϣ��
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
