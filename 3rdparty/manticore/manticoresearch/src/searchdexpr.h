//
// Copyright (c) 2017-2020, Manticore Software LTD (http://manticoresearch.com)
// Copyright (c) 2001-2016, Andrew Aksyonoff
// Copyright (c) 2008-2016, Sphinx Technologies Inc
// All rights reserved
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License. You should have
// received a copy of the GPL license along with this program; if you
// did not, you can find it at http://www.gnu.org/
//

#ifndef _searchdexpr_
#define _searchdexpr_

#include "sphinx.h"
#include "sphinxexpr.h"

/// searchd expression hook
/// needed to implement functions that are builtin for searchd,
/// but can not be builtin in the generic expression engine itself,
/// like SNIPPET() function that must know about indexes, tokenizers, etc
class ExprHook_c : public ISphExprHook
{
public:
	int			IsKnownIdent ( const char * ) final	{ return -1; }
	int			IsKnownFunc ( const char * sFunc ) final;
	ISphExpr *	CreateNode ( int iID, ISphExpr * pLeft, ESphEvalStage * pEvalStage, CSphString & sError ) final;
	ESphAttr	GetIdentType ( int ) final;
	ESphAttr	GetReturnType ( int iID, const CSphVector<ESphAttr> & dArgs, bool, CSphString & sError ) final;
	void		CheckEnter ( int ) final {}
	void		CheckExit ( int ) final {}

	void		SetIndex ( CSphIndex * pIndex ) { m_pIndex = pIndex; }
	void		SetProfiler ( CSphQueryProfile * pProfiler ) { m_pProfiler = pProfiler; }
	void		SetQueryType ( QueryType_e eType ) { m_eQueryType = eType; }

private:
	CSphIndex *			m_pIndex = nullptr; /// BLOODY HACK
	CSphQueryProfile *	m_pProfiler = nullptr;
	QueryType_e			m_eQueryType = QUERY_API;
};


#endif // _searchdexpr_
