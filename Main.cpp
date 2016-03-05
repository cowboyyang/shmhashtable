#include <stdio.h>
#include "MShmHashTable.hpp"

class Actor
{
public:
    Actor() {}
    virtual ~Actor() {}
    int Init(){ m_iGameID = 0; }
    int Resume() {}
    int Reclaim() {}
    virtual void Laugh() = 0;
    void SetGameID(int iGameID) { m_iGameID = iGameID; }
    int GetGameID() { return m_iGameID; }

private:
    int m_iGameID;
};

class Person : public Actor
{
public:
   Person() {}
   virtual ~Person() {}
   int Init() { Actor::Init(); }
   int Resume() { Actor::Resume(); }
   int Reclaim() { Actor::Reclaim(); }

   void Laugh() 
   { 
       printf("person gameid(%d) is laugh!\n", GetGameID());
   }
private:
};

int main()
{
    MShmHashTable<int, Person> hashTable;
    MHashConfig stConf;
    stConf.shmKey = 12345;
    stConf.dwMaxDataNodeNum = 1000;
    stConf.dwMaxHashBucketsNum = 1000;
    
    MHashErrorCode ret = hashTable.InitHashTable(stConf);
    if ( ENM_MHASH_STATUS_OK != ret )
    {
        printf("init hash talbe failed, ret(%d : %s)\n", ret, 
                hashTable.GetErrorString(ret) );
        return -1;
    }

    MHashCreateMode mode = hashTable.GetStartMode();
    printf("start mode (%d)\n", mode);

    if ( ENM_MHASH_SHM_INIT == mode )
    {
        Person p;
        p.SetGameID(666);
        printf("game id: %d\n", p.GetGameID());
        hashTable.Insert(1000, p);
    }
    else
    {

        Person *p = hashTable.Find(1000, ret);
        if ( NULL == p )
        {
            printf("get key(%d) failed, ret(%d:%s)\n", 
                    1000, ret, hashTable.GetErrorString(ret));
            return -1;
        }

        printf("person gameid(%d)\n", p->GetGameID());
        p->Laugh();
    }
    return 0;
}
