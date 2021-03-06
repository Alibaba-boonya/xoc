/*@
XOC Release License

Copyright (c) 2013-2014, Alibaba Group, All rights reserved.

    compiler@aliexpress.com

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Su Zhenyu nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

author: Su Zhenyu
@*/
#ifndef _IPA_H_
#define _IPA_H_

namespace xoc {

class IPA : public Pass {
protected:
	RegionMgr * m_ru_mgr;
	SMemPool * m_pool;

	void * xmalloc(UINT size)
	{
		void * p = smpoolMalloc(size, m_pool);
		ASSERT0(p);
		memset(p, 0, size);
		return p;
	}
public:
	IPA(RegionMgr * ru_mgr)
	{
		m_ru_mgr = ru_mgr;
		m_pool = smpoolCreate(16, MEM_COMM);
	}
	virtual ~IPA() { smpoolDelete(m_pool); }
	virtual CHAR const* get_pass_name() const { return "IPA"; }
	virtual PASS_TYPE get_pass_type() const { return PASS_IPA; }
	virtual bool perform(OptCTX & oc);
};

} //namespace xoc
#endif
