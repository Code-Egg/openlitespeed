/*****************************************************************************
*    Open LiteSpeed is an open source HTTP server.                           *
*    Copyright (C) 2013 - 2022  LiteSpeed Technologies, Inc.                 *
*                                                                            *
*    This program is free software: you can redistribute it and/or modify    *
*    it under the terms of the GNU General Public License as published by    *
*    the Free Software Foundation, either version 3 of the License, or       *
*    (at your option) any later version.                                     *
*                                                                            *
*    This program is distributed in the hope that it will be useful,         *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of          *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the            *
*    GNU General Public License for more details.                            *
*                                                                            *
*    You should have received a copy of the GNU General Public License       *
*    along with this program. If not, see http://www.gnu.org/licenses/.      *
*****************************************************************************/
#ifndef EDIOSTREAM_H
#define EDIOSTREAM_H


#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <edio/eventreactor.h>
#include <edio/flowcontrol.h>
#include <edio/inputstream.h>
#include <edio/multiplexer.h>
#include <edio/outputstream.h>
#include <util/iovec.h>

class EdIoStream : public InputStream, public OutputStream
                 , virtual public IoFlowControl
{
public:
    EdIoStream()  {}
    virtual ~EdIoStream() {}
    virtual int shutdown()  {   return -1;      }
    virtual int doneWrite() = 0;
    virtual int sendfile(int fdSrc, off_t off, size_t size, int flag)
    { return -1; }
    virtual int onRead()            {   return 0;   }
    virtual int onWrite()           {   return 0;   }
    virtual int onPeerShutdown()    {   return 0;   }
    virtual int onPeerClose()       {   return 0;   }
    virtual int onClose()           {   return 0;   }
    virtual int onAbort()           {   return 0;   }
};


class Multiplexer;
class LoopBuf;
class EdStream : public EventReactor, virtual public EdIoStream
{
    Multiplexer *m_pMplex;

    EdStream(const EdStream &rhs);
    void operator=(const EdStream &rhs);
    int regist(Multiplexer *pMplx, int event = 0);
protected:
    virtual int handleEvents(short event);
    void setMultiplexer(Multiplexer *pMplx)
    {   m_pMplex = pMplx; }
public:
    EdStream();
    EdStream(int fd, Multiplexer *pMplx, int events = 0);
    ~EdStream();
    void init(int fd, Multiplexer *pMplx, int events)
    {
        setfd(fd);
        m_pMplex = pMplx;
        regist(pMplx, events);
    }

    void takeover(EdStream *old)
    {
        setfd(old->getfd());
        m_pMplex = old->getCurMplex();
        m_pMplex->replace(old, this);
    }

    virtual void continueRead();
    virtual void suspendRead();

    int read(char *pBuf, int size);
    int readv(struct iovec *vector, int count);
    virtual int onRead() = 0;

    virtual void continueWrite();
    virtual void suspendWrite();
    virtual int onWrite() = 0;
    int write(const char *buf, int len)
    {
        int ret;
        while (1)
        {
            ret = ::write(getfd(), buf, len);
            if (ret == -1)
            {
                if (errno == EAGAIN)
                    ret = 0;
                if (errno == EINTR)
                    continue;
            }
            if (ret < len)
                resetRevent(POLLOUT);
            return ret;
        }
    }

    int writev(const struct iovec *iov, int count)
    {
        int ret;
        while (1)
        {
            ret = ::writev(getfd(), iov, count);
            if (ret == -1)
            {
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN)
                {
                    resetRevent(POLLOUT);
                    ret = 0;
                }
            }
            return ret;
        }
    }

    int writev(IOVec &vector)
    {
        return writev(vector.get(), vector.len());
    }

    int writev(IOVec &vector, int total)
    {   return writev(vector);        }

    int write(LoopBuf *pBuf);

    Multiplexer *getCurMplex() const
    {   return m_pMplex;  }

    virtual int onHangup();
    virtual int onError() = 0;
    virtual int onEventDone(short event) = 0;
//    virtual bool wantRead() = 0;
//    virtual bool wantWrite() = 0;
//    void updateEvents();
    int doneWrite()
    {   return -1;      }
    int close();
    int flush()    {   return LS_OK;    }
    int getSockError(int32_t *error);

    void suspendNotify();
    void resumeNotify();

};


#endif
