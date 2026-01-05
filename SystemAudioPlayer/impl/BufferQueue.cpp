/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2024 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#include "BufferQueue.h"
#include <cstring>

void Buffer::fillBuffer(const void *ptr,int len)
{
    this->length = len;
    buff = new char[length];
    std::memcpy(buff,ptr,length);
}

int Buffer::getLength()
{
    return length;
}

char* Buffer::getBuffer()
{
    return buff;
}

void Buffer::deleteBuffer()
{  
    SAPLOG_TRACE("SAP: delete Buffer..."); 
    if(buff != NULL)
    {
       delete[] buff;
       buff = NULL;
    }
}

BufferQueue::BufferQueue(int size)
{
    pthread_mutex_init(&m_mutex, NULL);
    sem_init(&m_sem_full,0,0);
    sem_init(&m_sem_empty,0,size);
}

BufferQueue::~BufferQueue()
{
    pthread_mutex_destroy(&m_mutex);
    sem_destroy(&m_sem_empty);
    sem_destroy(&m_sem_full);
}
void BufferQueue::preDelete()
{
    clear();
    sem_post(&m_sem_full);           
}

void BufferQueue::add(Buffer *data)
{
    sem_wait(&m_sem_empty);
    pthread_mutex_lock(&m_mutex);
    m_buffer.push(data);
    sem_post(&m_sem_full);
    pthread_mutex_unlock(&m_mutex);
}

int BufferQueue::count()
{
    int count;
    pthread_mutex_lock(&m_mutex);
    //count = m_buffer.size();
    sem_getvalue(&m_sem_full,&count); 
    pthread_mutex_unlock(&m_mutex);
    return count;
}

void BufferQueue::clear()
{
    Buffer *item;
    int value;
    pthread_mutex_lock(&m_mutex);
    while(!m_buffer.empty())
    {
        item = m_buffer.front();
        m_buffer.pop();
        item->deleteBuffer();
        delete item;
        sem_getvalue(&m_sem_full,&value);
        if(value != 0) 
		{
            pthread_mutex_unlock(&m_mutex);
            sem_wait(&m_sem_full);
            pthread_mutex_lock(&m_mutex);
        }
        sem_post(&m_sem_empty);
    }
    pthread_mutex_unlock(&m_mutex);
}

bool BufferQueue::isFull()
{
    int value;
    sem_getvalue(&m_sem_empty,&value);
    if(value == 0)
    {
       return true;
    }
    return false;
}

bool BufferQueue::isEmpty()
{
    pthread_mutex_lock(&m_mutex);
    if(m_buffer.empty())
    {
       pthread_mutex_unlock(&m_mutex);
       return true;
    }  
    pthread_mutex_unlock(&m_mutex);
    return false;
}

Buffer* BufferQueue::remove()
{
    Buffer *item = NULL;
    sem_wait(&m_sem_full);
    pthread_mutex_lock(&m_mutex);
    if(!m_buffer.empty())
    {
        item = m_buffer.front();
        m_buffer.pop();
        sem_post(&m_sem_empty);
    }
    pthread_mutex_unlock(&m_mutex);
    return item;
}
