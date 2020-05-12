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

#include "searchdsql.h"
#include "sphinxint.h"
#include "sphinxplugin.h"
#include "searchdaemon.h"
#include "searchdddl.h"

extern int g_iAgentQueryTimeout;	// global (default). May be override by index-scope values, if one specified

#define YYSTYPE SqlNode_t

									// unused parameter, simply to avoid type clash between all my yylex() functions
#define YY_DECL static int my_lex ( YYSTYPE * lvalp, void * yyscanner, SqlParser_c * pParser )

#if USE_WINDOWS
#define YY_NO_UNISTD_H 1
#endif

#ifdef CMAKE_GENERATED_LEXER
	#ifdef __GNUC__
		#pragma GCC diagnostic push 
		#pragma GCC diagnostic ignored "-Wsign-compare"
		#pragma GCC diagnostic ignored "-Wpragmas"
		#pragma GCC diagnostic ignored "-Wunneeded-internal-declaration"
	#endif

	#include "flexsphinxql.c"

	#ifdef __GNUC__
		#pragma GCC diagnostic pop
	#endif

#else
	#include "llsphinxql.c"
#endif

void yyerror ( SqlParser_c * pParser, const char * sMessage )
{
	// flex put a zero at last token boundary; make it undo that
	yylex_unhold ( pParser->m_pScanner );

	// create our error message
	pParser->m_pParseError->SetSprintf ( "%s %s near '%s'", pParser->m_sErrorHeader.cstr(), sMessage,
		pParser->m_pLastTokenStart ? pParser->m_pLastTokenStart : "(null)" );

	// fixup TOK_xxx thingies
	char * s = const_cast<char*> ( pParser->m_pParseError->cstr() );
	char * d = s;
	while ( *s )
	{
		if ( strncmp ( s, "TOK_", 4 )==0 )
			s += 4;
		else
			*d++ = *s++;
	}
	*d = '\0';
}


#ifndef NDEBUG
// using a proxy to be possible to debug inside yylex
static int yylex ( YYSTYPE * lvalp, SqlParser_c * pParser )
{
	int res = my_lex ( lvalp, pParser->m_pScanner, pParser );
	return res;
}
#else
static int yylex ( YYSTYPE * lvalp, SqlParser_c * pParser )
{
	return my_lex ( lvalp, pParser->m_pScanner, pParser );
}
#endif

#ifdef CMAKE_GENERATED_GRAMMAR
	#include "bissphinxql.c"
#else
	#include "yysphinxql.c"
#endif

//////////////////////////////////////////////////////////////////////////

SqlStmt_t::SqlStmt_t()
{
	m_tQuery.m_eMode = SPH_MATCH_EXTENDED2; // only new and shiny matching and sorting
	m_tQuery.m_eSort = SPH_SORT_EXTENDED;
	m_tQuery.m_sSortBy = "@weight desc"; // default order
	m_tQuery.m_sOrderBy = "@weight desc";
	m_tQuery.m_iAgentQueryTimeout = g_iAgentQueryTimeout;
	m_tQuery.m_iRetryCount = -1;
	m_tQuery.m_iRetryDelay = -1;
}


SqlStmt_t::~SqlStmt_t()
{
	SafeDelete ( m_pTableFunc );
}


bool SqlStmt_t::AddSchemaItem ( const char * psName )
{
	m_dInsertSchema.Add ( psName );
	CSphString & sAttr = m_dInsertSchema.Last();
	sAttr.ToLower();
	int iLen = sAttr.Length();
	if ( iLen>1 && sAttr.cstr()[0] == '`' && sAttr.cstr()[iLen-1]=='`' )
		sAttr = sAttr.SubString ( 1, iLen-2 );

	m_iSchemaSz = m_dInsertSchema.GetLength();
	return true; // stub; check if the given field actually exists in the schema
}

// check if the number of fields which would be inserted is in accordance to the given schema
bool SqlStmt_t::CheckInsertIntegrity()
{
	// cheat: if no schema assigned, assume the size of schema as the size of the first row.
	// (if it is wrong, it will be revealed later)
	if ( !m_iSchemaSz )
		m_iSchemaSz = m_dInsertValues.GetLength();

	m_iRowsAffected++;
	return m_dInsertValues.GetLength()==m_iRowsAffected*m_iSchemaSz;
}

//////////////////////////////////////////////////////////////////////////

SqlParserTraits_c::SqlParserTraits_c ( CSphVector<SqlStmt_t> & dStmt )
	: m_dStmt ( dStmt )
{}


void SqlParserTraits_c::PushQuery()
{
	assert ( m_dStmt.GetLength() || ( !m_pQuery && !m_pStmt ) );

	// add new
	m_dStmt.Add ( SqlStmt_t() );
	m_pStmt = &m_dStmt.Last();
}


CSphString & SqlParserTraits_c::ToString ( CSphString & sRes, const SqlNode_t & tNode ) const
{
	if ( tNode.m_iType>=0 )
		sRes.SetBinary ( m_pBuf + tNode.m_iStart, tNode.m_iEnd - tNode.m_iStart );
	else switch ( tNode.m_iType )
	{
	case SPHINXQL_TOK_COUNT:	sRes = "@count"; break;
	case SPHINXQL_TOK_GROUPBY:	sRes = "@groupby"; break;
	case SPHINXQL_TOK_WEIGHT:	sRes = "@weight"; break;
	default:					assert ( 0 && "internal error: unknown parser ident code" );
	}
	return sRes;
}


CSphString SqlParserTraits_c::ToStringUnescape ( const SqlNode_t & tNode ) const
{
	assert ( tNode.m_iType>=0 );
	return SqlUnescape ( m_pBuf + tNode.m_iStart, tNode.m_iEnd - tNode.m_iStart );
}

//////////////////////////////////////////////////////////////////////////

SqlParser_c::SqlParser_c ( CSphVector<SqlStmt_t> & dStmt, ESphCollation eCollation )
	: SqlParserTraits_c ( dStmt )
	, m_eCollation ( eCollation )
{
	assert ( !m_dStmt.GetLength() );
	PushQuery ();
}

void SqlParser_c::PushQuery ()
{
	assert ( m_dStmt.GetLength() || ( !m_pQuery && !m_pStmt ) );

	// post set proper result-set order
	if ( m_dStmt.GetLength() && m_pQuery )
	{
		if ( m_pQuery->m_sGroupBy.IsEmpty() )
			m_pQuery->m_sSortBy = m_pQuery->m_sOrderBy;
		else
			m_pQuery->m_sGroupSortBy = m_pQuery->m_sOrderBy;

		m_dFiltersPerStmt.Add ( m_dFilterTree.GetLength() );
	}

	SqlParserTraits_c::PushQuery();

	m_pQuery = &m_pStmt->m_tQuery;
	m_pQuery->m_eCollation = m_eCollation;

	m_bGotQuery = false;
}


bool SqlParser_c::AddOption ( const SqlNode_t & tIdent )
{
	CSphString sOpt, sVal;
	ToString ( sOpt, tIdent ).ToLower();

	if ( sOpt=="low_priority" )
		m_pQuery->m_bLowPriority = true;
	else if ( sOpt=="debug_no_payload" )
		m_pStmt->m_tQuery.m_uDebugFlags |= QUERY_DEBUG_NO_PAYLOAD;
	else
	{
		m_pParseError->SetSprintf ( "unknown option '%s'", sOpt.cstr() );
		return false;
	}

	return true;
}


bool SqlParser_c::CheckInteger ( const CSphString & sOpt, const CSphString & sVal ) const
{
	const char * p = sVal.cstr();
	while ( sphIsInteger ( *p++ ) )
		p++;

	if ( *p )
	{
		m_pParseError->SetSprintf ( "%s value should be a number: '%s'", sOpt.cstr(), sVal.cstr() );
		return false;
	}

	return true;
}


bool SqlParser_c::AddOption ( const SqlNode_t & tIdent, const SqlNode_t & tValue )
{
	CSphString sOpt, sVal;
	ToString ( sOpt, tIdent ).ToLower();
	ToString ( sVal, tValue ).ToLower().Unquote();

	// OPTIMIZE? hash possible sOpt choices?
	if ( sOpt=="ranker" )
	{
		m_pQuery->m_eRanker = SPH_RANK_TOTAL;
		for ( int iRanker = SPH_RANK_PROXIMITY_BM25; iRanker<=SPH_RANK_SPH04; iRanker++ )
			if ( sVal==sphGetRankerName ( ESphRankMode ( iRanker ) ) )
			{
				m_pQuery->m_eRanker = ESphRankMode ( iRanker );
				break;
			}

		if ( m_pQuery->m_eRanker==SPH_RANK_TOTAL )
		{
			if ( sVal==sphGetRankerName ( SPH_RANK_EXPR ) || sVal==sphGetRankerName ( SPH_RANK_EXPORT ) )
			{
				m_pParseError->SetSprintf ( "missing ranker expression (use OPTION ranker=expr('1+2') for example)" );
				return false;
			} else if ( sphPluginExists ( PLUGIN_RANKER, sVal.cstr() ) )
			{
				m_pQuery->m_eRanker = SPH_RANK_PLUGIN;
				m_pQuery->m_sUDRanker = sVal;
			}
			m_pParseError->SetSprintf ( "unknown ranker '%s'", sVal.cstr() );
			return false;
		}
	} else if ( sOpt=="token_filter" )	// tokfilter = hello.dll:hello:some_opts
	{
		StrVec_t dParams;
		if ( !sphPluginParseSpec ( sVal, dParams, *m_pParseError ) )
			return false;

		if ( !dParams.GetLength() )
		{
			m_pParseError->SetSprintf ( "missing token filter spec string" );
			return false;
		}

		m_pQuery->m_sQueryTokenFilterLib = dParams[0];
		m_pQuery->m_sQueryTokenFilterName = dParams[1];
		m_pQuery->m_sQueryTokenFilterOpts = dParams[2];
	} else if ( sOpt=="max_matches" )
	{
		if ( !CheckInteger ( sOpt, sVal ) )
			return false;

		m_pQuery->m_iMaxMatches = (int)tValue.m_iValue;

	} else if ( sOpt=="cutoff" )
	{
		if ( !CheckInteger ( sOpt, sVal ) )
			return false;

		m_pQuery->m_iCutoff = (int)tValue.m_iValue;

	} else if ( sOpt=="max_query_time" )
	{
		if ( !CheckInteger ( sOpt, sVal ) )
			return false;

		m_pQuery->m_uMaxQueryMsec = (int)tValue.m_iValue;

	} else if ( sOpt=="retry_count" )
	{
		if ( !CheckInteger ( sOpt, sVal ) )
			return false;

		m_pQuery->m_iRetryCount = (int)tValue.m_iValue;

	} else if ( sOpt=="retry_delay" )
	{
		if ( !CheckInteger ( sOpt, sVal ) )
			return false;

		m_pQuery->m_iRetryDelay = (int)tValue.m_iValue;

	} else if ( sOpt=="reverse_scan" )
	{
		if ( !CheckInteger ( sOpt, sVal ) )
			return false;

		m_pQuery->m_bReverseScan = ( tValue.m_iValue!=0 );

	} else if ( sOpt=="ignore_nonexistent_columns" )
	{
		if ( !CheckInteger ( sOpt, sVal ) )
			return false;

		m_pQuery->m_bIgnoreNonexistent = ( tValue.m_iValue!=0 );

	} else if ( sOpt=="comment" )
	{
		m_pQuery->m_sComment = ToStringUnescape ( tValue );

	} else if ( sOpt=="sort_method" )
	{
		if ( sVal=="pq" )			m_pQuery->m_bSortKbuffer = false;
		else if ( sVal=="kbuffer" )	m_pQuery->m_bSortKbuffer = true;
		else
		{
			m_pParseError->SetSprintf ( "unknown sort_method=%s (known values are pq, kbuffer)", sVal.cstr() );
			return false;
		}

	} else if ( sOpt=="agent_query_timeout" )
	{
		if ( !CheckInteger ( sOpt, sVal ) )
			return false;

		m_pQuery->m_iAgentQueryTimeout = (int)tValue.m_iValue;

	} else if ( sOpt=="max_predicted_time" )
	{
		if ( !CheckInteger ( sOpt, sVal ) )
			return false;

		m_pQuery->m_iMaxPredictedMsec = int ( tValue.m_iValue > INT_MAX ? INT_MAX : tValue.m_iValue );

	} else if ( sOpt=="boolean_simplify" )
	{
		m_pQuery->m_bSimplify = true;

	} else if ( sOpt=="idf" )
	{
		StrVec_t dOpts;
		sphSplit ( dOpts, sVal.cstr() );

		ARRAY_FOREACH ( i, dOpts )
		{
			if ( dOpts[i]=="normalized" )
				m_pQuery->m_bPlainIDF = false;
			else if ( dOpts[i]=="plain" )
				m_pQuery->m_bPlainIDF = true;
			else if ( dOpts[i]=="tfidf_normalized" )
				m_pQuery->m_bNormalizedTFIDF = true;
			else if ( dOpts[i]=="tfidf_unnormalized" )
				m_pQuery->m_bNormalizedTFIDF = false;
			else
			{
				m_pParseError->SetSprintf ( "unknown flag %s in idf=%s (known values are plain, normalized, tfidf_normalized, tfidf_unnormalized)",
					dOpts[i].cstr(), sVal.cstr() );
				return false;
			}
		}
	} else if ( sOpt=="global_idf" )
	{
		if ( !CheckInteger ( sOpt, sVal ) )
			return false;

		m_pQuery->m_bGlobalIDF = ( tValue.m_iValue!=0 );

	} else if ( sOpt=="local_df" )
	{
		if ( !CheckInteger ( sOpt, sVal ) )
			return false;

		m_pQuery->m_bLocalDF = ( tValue.m_iValue!=0 );

	} else if ( sOpt=="ignore_nonexistent_indexes" )
	{
		if ( !CheckInteger ( sOpt, sVal ) )
			return false;

		m_pQuery->m_bIgnoreNonexistentIndexes = ( tValue.m_iValue!=0 );

	} else if ( sOpt=="strict" )
	{
		if ( !CheckInteger ( sOpt, sVal ) )
			return false;

		m_pQuery->m_bStrict = ( tValue.m_iValue!=0 );

	} else if ( sOpt=="columns" ) // for SHOW THREADS
	{
		if ( !CheckInteger ( sOpt, sVal ) )
			return false;

		m_pStmt->m_iThreadsCols = Max ( (int)tValue.m_iValue, 0 );

	} else if ( sOpt=="rand_seed" )
	{
		if ( !CheckInteger ( sOpt, sVal ) )
			return false;

		m_pStmt->m_tQuery.m_iRandSeed = int64_t(DWORD(tValue.m_iValue));

	} else if ( sOpt=="sync" )
	{
		if ( !CheckInteger ( sOpt, sVal ) )
			return false;

		m_pQuery->m_bSync = ( tValue.m_iValue!=0 );

	} else if ( sOpt=="expand_keywords" )
	{
		if ( !CheckInteger ( sOpt, sVal ) )
			return false;

		m_pQuery->m_eExpandKeywords = ( tValue.m_iValue!=0 ? QUERY_OPT_ENABLED : QUERY_OPT_DISABLED );

	} else if ( sOpt=="format" ) // for SHOW THREADS
	{
		m_pStmt->m_sThreadFormat = sVal;

	} else if ( sOpt=="morphology" )
	{
		if ( sVal=="none" )
		{
			m_pQuery->m_eExpandKeywords = QUERY_OPT_MORPH_NONE;
		} else
		{
			m_pParseError->SetSprintf ( "morphology could be only disabled with option none, got %s", sVal.cstr() );
			return false;
		}

	} else
	{
		m_pParseError->SetSprintf ( "unknown option '%s' (or bad argument type)", sOpt.cstr() );
		return false;
	}

	return true;
}


bool SqlParser_c::AddOption ( const SqlNode_t & tIdent, const SqlNode_t & tValue, const SqlNode_t & tArg )
{
	CSphString sOpt, sVal;
	ToString ( sOpt, tIdent ).ToLower();
	ToString ( sVal, tValue ).ToLower().Unquote();

	if ( sOpt=="ranker" )
	{
		if ( sVal=="expr" || sVal=="export" )
		{
			m_pQuery->m_eRanker = sVal=="expr" ? SPH_RANK_EXPR : SPH_RANK_EXPORT;
			m_pQuery->m_sRankerExpr = ToStringUnescape ( tArg );
			return true;
		} else if ( sphPluginExists ( PLUGIN_RANKER, sVal.cstr() ) )
		{
			m_pQuery->m_eRanker = SPH_RANK_PLUGIN;
			m_pQuery->m_sUDRanker = sVal;
			m_pQuery->m_sUDRankerOpts = ToStringUnescape ( tArg );
			return true;
		}
	}

	m_pParseError->SetSprintf ( "unknown option or extra argument to '%s=%s'", sOpt.cstr(), sVal.cstr() );
	return false;
}


bool SqlParser_c::AddOption ( const SqlNode_t & tIdent, CSphVector<CSphNamedInt> & dNamed )
{
	CSphString sOpt;
	ToString ( sOpt, tIdent ).ToLower ();

	if ( sOpt=="field_weights" )
	{
		m_pQuery->m_dFieldWeights.SwapData ( dNamed );

	} else if ( sOpt=="index_weights" )
	{
		m_pQuery->m_dIndexWeights.SwapData ( dNamed );

	} else
	{
		m_pParseError->SetSprintf ( "unknown option '%s' (or bad argument type)", sOpt.cstr() );
		return false;
	}

	return true;
}


bool SqlParser_c::AddInsertOption ( const SqlNode_t & tIdent, const SqlNode_t & tValue )
{
	CSphString sOpt, sVal;
	ToString ( sOpt, tIdent ).ToLower();
	ToString ( sVal, tValue ).Unquote();

	if ( sOpt=="token_filter_options" )
	{
		m_pStmt->m_sStringParam = sVal;
	} else
	{
		m_pParseError->SetSprintf ( "unknown option '%s' (or bad argument type)", sOpt.cstr() );
		return false;
	}
	return true;
}


void SqlParser_c::AddIndexHint ( IndexHint_e eHint, const SqlNode_t & tValue )
{
	CSphString sIndexes;
	ToString ( sIndexes, tValue );
	StrVec_t dIndexes;
	sphSplit ( dIndexes, sIndexes.cstr() );

	for ( const auto & i : dIndexes )
	{
		IndexHint_t & tHint = m_pQuery->m_dIndexHints.Add();
		tHint.m_sIndex = i;
		tHint.m_eHint = eHint;
	}
}


void SqlParser_c::AliasLastItem ( SqlNode_t * pAlias )
{
	if ( pAlias )
	{
		CSphQueryItem & tItem = m_pQuery->m_dItems.Last();
		tItem.m_sAlias.SetBinary ( m_pBuf + pAlias->m_iStart, pAlias->m_iEnd - pAlias->m_iStart );
		tItem.m_sAlias.ToLower();
		SetSelect ( pAlias );
	}
}


void SqlParser_c::AddInsval ( CSphVector<SqlInsert_t> & dVec, const SqlNode_t & tNode )
{
	SqlInsert_t & tIns = dVec.Add();
	tIns.m_iType = tNode.m_iType;
	tIns.m_iVal = tNode.m_iValue; // OPTIMIZE? copy conditionally based on type?
	tIns.m_fVal = tNode.m_fValue;
	if ( tIns.m_iType==TOK_QUOTED_STRING )
		tIns.m_sVal = ToStringUnescape ( tNode );
	tIns.m_pVals = tNode.m_pValues;
}


void SqlParser_c::ResetSelect()
{
	if ( m_pQuery )
		m_pQuery->m_iSQLSelectStart = m_pQuery->m_iSQLSelectEnd = -1;
}

void SqlParser_c::SetSelect ( SqlNode_t * pStart, SqlNode_t * pEnd )
{
	if ( m_pQuery )
	{
		if ( pStart && ( m_pQuery->m_iSQLSelectStart<0 || m_pQuery->m_iSQLSelectStart>pStart->m_iStart ) )
			m_pQuery->m_iSQLSelectStart = pStart->m_iStart;
		if ( !pEnd )
			pEnd = pStart;
		if ( pEnd && ( m_pQuery->m_iSQLSelectEnd<0 || m_pQuery->m_iSQLSelectEnd<pEnd->m_iEnd ) )
			m_pQuery->m_iSQLSelectEnd = pEnd->m_iEnd;
	}
}

void SqlParser_c::AutoAlias ( CSphQueryItem & tItem, SqlNode_t * pStart, SqlNode_t * pEnd )
{
	if ( pStart && pEnd )
	{
		tItem.m_sAlias.SetBinary ( m_pBuf + pStart->m_iStart, pEnd->m_iEnd - pStart->m_iStart );
		sphColumnToLowercase ( const_cast<char *>( tItem.m_sAlias.cstr() ) );
	} else
	{
		tItem.m_sAlias = tItem.m_sExpr;
	}
	SetSelect ( pStart, pEnd );
}

void SqlParser_c::AddItem ( SqlNode_t * pExpr, ESphAggrFunc eAggrFunc, SqlNode_t * pStart, SqlNode_t * pEnd )
{
	CSphQueryItem & tItem = m_pQuery->m_dItems.Add();
	tItem.m_sExpr.SetBinary ( m_pBuf + pExpr->m_iStart, pExpr->m_iEnd - pExpr->m_iStart );
	sphColumnToLowercase ( const_cast<char *>( tItem.m_sExpr.cstr() ) );
	tItem.m_eAggrFunc = eAggrFunc;
	AutoAlias ( tItem, pStart?pStart:pExpr, pEnd?pEnd:pExpr );
}

bool SqlParser_c::AddItem ( const char * pToken, SqlNode_t * pStart, SqlNode_t * pEnd )
{
	CSphQueryItem & tItem = m_pQuery->m_dItems.Add();
	tItem.m_sExpr = pToken;
	tItem.m_eAggrFunc = SPH_AGGR_NONE;
	sphColumnToLowercase ( const_cast<char *>( tItem.m_sExpr.cstr() ) );
	AutoAlias ( tItem, pStart, pEnd );
	return SetNewSyntax();
}

bool SqlParser_c::AddCount ()
{
	CSphQueryItem & tItem = m_pQuery->m_dItems.Add();
	tItem.m_sExpr = tItem.m_sAlias = "count(*)";
	tItem.m_eAggrFunc = SPH_AGGR_NONE;
	return SetNewSyntax();
}

void SqlParser_c::AddGroupBy ( const SqlNode_t & tGroupBy )
{
	if ( m_pQuery->m_sGroupBy.IsEmpty() )
	{
		m_pQuery->m_eGroupFunc = SPH_GROUPBY_ATTR;
		m_pQuery->m_sGroupBy.SetBinary ( m_pBuf + tGroupBy.m_iStart, tGroupBy.m_iEnd - tGroupBy.m_iStart );
		sphColumnToLowercase ( const_cast<char *>( m_pQuery->m_sGroupBy.cstr() ) );
	} else
	{
		m_pQuery->m_eGroupFunc = SPH_GROUPBY_MULTIPLE;
		CSphString sTmp;
		sTmp.SetBinary ( m_pBuf + tGroupBy.m_iStart, tGroupBy.m_iEnd - tGroupBy.m_iStart );
		sphColumnToLowercase ( const_cast<char *>( sTmp.cstr() ) );
		m_pQuery->m_sGroupBy.SetSprintf ( "%s, %s", m_pQuery->m_sGroupBy.cstr(), sTmp.cstr() );
	}
}

void SqlParser_c::SetGroupbyLimit ( int iLimit )
{
	m_pQuery->m_iGroupbyLimit = iLimit;
}

bool SqlParser_c::AddDistinct ( SqlNode_t * pNewExpr, SqlNode_t * pStart, SqlNode_t * pEnd )
{
	if ( !m_pQuery->m_sGroupDistinct.IsEmpty() )
	{
		yyerror ( this, "too many COUNT(DISTINCT) clauses" );
		return false;
	}

	ToString ( m_pQuery->m_sGroupDistinct, *pNewExpr );
	return AddItem ( "@distinct", pStart, pEnd );
}

bool SqlParser_c::AddSchemaItem ( YYSTYPE * pNode )
{
	assert ( m_pStmt );
	CSphString sItem;
	sItem.SetBinary ( m_pBuf + pNode->m_iStart, pNode->m_iEnd - pNode->m_iStart );
	return m_pStmt->AddSchemaItem ( sItem.cstr() );
}

bool SqlParser_c::SetMatch ( const YYSTYPE& tValue )
{
	if ( m_bGotQuery )
	{
		yyerror ( this, "too many MATCH() clauses" );
		return false;
	};

	m_pQuery->m_sQuery = ToStringUnescape ( tValue );
	m_pQuery->m_sRawQuery = m_pQuery->m_sQuery;
	return m_bGotQuery = true;
}

void SqlParser_c::AddConst ( int iList, const YYSTYPE& tValue )
{
	CSphVector<CSphNamedInt> & dVec = GetNamedVec ( iList );

	dVec.Add();
	ToString ( dVec.Last().m_sName, tValue ).ToLower();
	dVec.Last().m_iValue = (int) tValue.m_iValue;
}

void SqlParser_c::SetStatement ( const YYSTYPE & tName, SqlSet_e eSet )
{
	m_pStmt->m_eStmt = STMT_SET;
	m_pStmt->m_eSet = eSet;
	ToString ( m_pStmt->m_sSetName, tName );
}

void SqlParser_c::GenericStatement ( SqlNode_t * pNode, SqlStmt_e iStmt )
{
	m_pStmt->m_eStmt = iStmt;
	m_pStmt->m_iListStart = pNode->m_iStart;
	m_pStmt->m_iListEnd = pNode->m_iEnd;
	ToString ( m_pStmt->m_sIndex, *pNode );
}

bool SqlParser_c::UpdateStatement ( SqlNode_t * pNode )
{
	GenericStatement ( pNode, STMT_UPDATE );
	SetIndex ( *pNode );
	m_pStmt->m_tUpdate.m_dRowOffset.Add ( 0 );
	return true;
}

bool SqlParser_c::DeleteStatement ( SqlNode_t * pNode )
{
	GenericStatement ( pNode, STMT_DELETE );
	SetIndex ( *pNode );
	return true;
}

void SqlParser_c::AddUpdatedAttr ( const SqlNode_t & tName, ESphAttr eType ) const
{
	CSphAttrUpdate & tUpd = m_pStmt->m_tUpdate;
	CSphString sAttr;
	TypedAttribute_t & tNew = tUpd.m_dAttributes.Add();
	tNew.m_sName = ToString ( sAttr, tName ).ToLower();
	tNew.m_eType = eType;
}


void SqlParser_c::UpdateMVAAttr ( const SqlNode_t & tName, const SqlNode_t & dValues )
{
	CSphAttrUpdate & tUpd = m_pStmt->m_tUpdate;
	ESphAttr eType = SPH_ATTR_UINT32SET;

	if ( dValues.m_pValues && dValues.m_pValues->GetLength()>0 )
	{
		// got MVA values, let's process them
		dValues.m_pValues->Uniq(); // don't need dupes within MVA
		tUpd.m_dPool.Add ( dValues.m_pValues->GetLength()*2 );
		for ( auto uVal : *dValues.m_pValues )
		{
			if ( uVal>UINT_MAX )
				eType = SPH_ATTR_INT64SET;
			*(( int64_t* ) tUpd.m_dPool.AddN ( 2 )) = uVal;
		}
	} else
	{
		// no values, means we should delete the attribute
		// we signal that to the update code by putting a single zero
		// to the values pool (meaning a zero-length MVA values list)
		tUpd.m_dPool.Add ( 0 );
	}

	AddUpdatedAttr ( tName, eType );
}


void SqlParser_c::UpdateStringAttr ( const SqlNode_t & tCol, const SqlNode_t & tStr )
{
	CSphAttrUpdate & tUpd = m_pStmt->m_tUpdate;

	auto sStr = ToStringUnescape ( tStr );

	int iLength = sStr.Length();
	tUpd.m_dPool.Add ( tUpd.m_dBlobs.GetLength() );
	tUpd.m_dPool.Add ( iLength );

	if ( iLength )
	{
		BYTE * pBlob = tUpd.m_dBlobs.AddN ( iLength+2 );	// a couple of extra \0 for json parser to be happy
		memcpy ( pBlob, sStr.cstr(), iLength );
		pBlob[iLength] = 0;
		pBlob[iLength+1] = 0;
	}

	AddUpdatedAttr ( tCol, SPH_ATTR_STRING );
}


CSphFilterSettings * SqlParser_c::AddFilter ( const SqlNode_t & tCol, ESphFilter eType )
{
	CSphString sCol;
	ToString ( sCol, tCol ); // do NOT lowercase just yet, might have to retain case for JSON cols

	FilterTreeItem_t & tElem = m_dFilterTree.Add();
	tElem.m_iFilterItem = m_pQuery->m_dFilters.GetLength();

	CSphFilterSettings * pFilter = &m_pQuery->m_dFilters.Add();
	pFilter->m_sAttrName = sCol;
	pFilter->m_eType = eType;
	sphColumnToLowercase ( const_cast<char *>( pFilter->m_sAttrName.cstr() ) );
	return pFilter;
}

bool SqlParser_c::AddFloatRangeFilter ( const SqlNode_t & sAttr, float fMin, float fMax, bool bHasEqual, bool bExclude )
{
	CSphFilterSettings * pFilter = AddFilter ( sAttr, SPH_FILTER_FLOATRANGE );
	if ( !pFilter )
		return false;
	pFilter->m_fMinValue = fMin;
	pFilter->m_fMaxValue = fMax;
	pFilter->m_bHasEqualMin = bHasEqual;
	pFilter->m_bHasEqualMax = bHasEqual;
	pFilter->m_bExclude = bExclude;
	return true;
}

bool SqlParser_c::AddIntRangeFilter ( const SqlNode_t & sAttr, int64_t iMin, int64_t iMax, bool bExclude )
{
	CSphFilterSettings * pFilter = AddFilter ( sAttr, SPH_FILTER_RANGE );
	if ( !pFilter )
		return false;
	pFilter->m_iMinValue = iMin;
	pFilter->m_iMaxValue = iMax;
	pFilter->m_bExclude = bExclude;
	return true;
}

bool SqlParser_c::AddIntFilterGreater ( const SqlNode_t & tAttr, int64_t iVal, bool bHasEqual )
{
	CSphFilterSettings * pFilter = AddFilter ( tAttr, SPH_FILTER_RANGE );
	if ( !pFilter )
		return false;

	pFilter->m_iMaxValue = LLONG_MAX;
	pFilter->m_iMinValue = iVal;
	pFilter->m_bHasEqualMin = bHasEqual;
	pFilter->m_bOpenRight = true;

	return true;
}

bool SqlParser_c::AddIntFilterLesser ( const SqlNode_t & tAttr, int64_t iVal, bool bHasEqual )
{
	CSphFilterSettings * pFilter = AddFilter ( tAttr, SPH_FILTER_RANGE );
	if ( !pFilter )
		return false;

	pFilter->m_iMinValue = LLONG_MIN;
	pFilter->m_iMaxValue = iVal;
	pFilter->m_bHasEqualMax = bHasEqual;
	pFilter->m_bOpenLeft = true;

	return true;
}

bool SqlParser_c::AddUservarFilter ( const SqlNode_t & tCol, const SqlNode_t & tVar, bool bExclude )
{
	CSphFilterSettings * pFilter = AddFilter ( tCol, SPH_FILTER_USERVAR );
	if ( !pFilter )
		return false;

	CSphString & sUserVar = pFilter->m_dStrings.Add();
	ToString ( sUserVar, tVar ).ToLower();
	pFilter->m_bExclude = bExclude;
	return true;
}


bool SqlParser_c::AddStringFilter ( const SqlNode_t & tCol, const SqlNode_t & tVal, bool bExclude )
{
	CSphFilterSettings * pFilter = AddFilter ( tCol, SPH_FILTER_STRING );
	if ( !pFilter )
		return false;
	CSphString & sFilterString = pFilter->m_dStrings.Add();
	sFilterString = ToStringUnescape ( tVal );
	pFilter->m_bExclude = bExclude;
	return true;
}


bool SqlParser_c::AddStringListFilter ( const SqlNode_t & tCol, SqlNode_t & tVal, StrList_e eType, bool bInverse )
{
	CSphFilterSettings * pFilter = AddFilter ( tCol, SPH_FILTER_STRING_LIST );
	if ( !pFilter || !tVal.m_pValues )
		return false;

	pFilter->m_dStrings.Resize ( tVal.m_pValues->GetLength() );
	ARRAY_FOREACH ( i, ( *tVal.m_pValues ) )
	{
		uint64_t uVal = ( *tVal.m_pValues )[i];
		int iOff = ( uVal>>32 );
		int iLen = ( uVal & 0xffffffff );
		pFilter->m_dStrings[i] = SqlUnescape ( m_pBuf + iOff, iLen );
	}
	tVal.m_pValues = nullptr;
	pFilter->m_bExclude = bInverse;
	assert ( pFilter->m_eMvaFunc == SPH_MVAFUNC_NONE ); // that is default for IN filter
	if ( eType==StrList_e::STR_ANY )
		pFilter->m_eMvaFunc = SPH_MVAFUNC_ANY;
	else if ( eType==StrList_e::STR_ALL )
		pFilter->m_eMvaFunc = SPH_MVAFUNC_ALL;
	return true;
}


bool SqlParser_c::AddNullFilter ( const SqlNode_t & tCol, bool bEqualsNull )
{
	CSphFilterSettings * pFilter = AddFilter ( tCol, SPH_FILTER_NULL );
	if ( !pFilter )
		return false;
	pFilter->m_bIsNull = bEqualsNull;
	return true;
}

void SqlParser_c::AddHaving ()
{
	assert ( m_pQuery->m_dFilters.GetLength() );
	m_pQuery->m_tHaving = m_pQuery->m_dFilters.Pop();
}


bool SqlParser_c::IsGoodSyntax()
{
	if ( ( m_uSyntaxFlags & 3 )!=3 )
		return true;
	yyerror ( this, "Mixing the old-fashion internal vars (@id, @count, @weight) with new acronyms like count(*), weight() is prohibited" );
	return false;
}


int SqlParser_c::AllocNamedVec ()
{
	// we only allow one such vector at a time, right now
	assert ( !m_bNamedVecBusy );
	m_bNamedVecBusy = true;
	m_dNamedVec.Resize ( 0 );
	return 0;
}

void SqlParser_c::SetLimit ( int iOffset, int iLimit )
{
	m_pQuery->m_iOffset = iOffset;
	m_pQuery->m_iLimit = iLimit;
}

#ifndef NDEBUG
CSphVector<CSphNamedInt> & SqlParser_c::GetNamedVec ( int iIndex )
#else
CSphVector<CSphNamedInt> & SqlParser_c::GetNamedVec ( int )
#endif
{
	assert ( m_bNamedVecBusy && iIndex==0 );
	return m_dNamedVec;
}

#ifndef NDEBUG
void SqlParser_c::FreeNamedVec ( int iIndex )
#else
void SqlParser_c::FreeNamedVec ( int )
#endif
{
	assert ( m_bNamedVecBusy && iIndex==0 );
	m_bNamedVecBusy = false;
	m_dNamedVec.Resize ( 0 );
}

void SqlParser_c::SetOp ( SqlNode_t & tNode )
{
	tNode.m_iParsedOp = m_dFilterTree.GetLength() - 1;
}


bool SqlParser_c::SetOldSyntax()
{
	m_uSyntaxFlags |= 1;
	return IsGoodSyntax ();
}


bool SqlParser_c::SetNewSyntax()
{
	m_uSyntaxFlags |= 2;
	return IsGoodSyntax();
}


bool SqlParser_c::IsDeprecatedSyntax() const
{
	return m_uSyntaxFlags & 1;
}


void SqlParser_c::FilterGroup ( SqlNode_t & tNode, SqlNode_t & tExpr )
{
	tNode.m_iParsedOp = tExpr.m_iParsedOp;
}

void SqlParser_c::FilterAnd ( SqlNode_t & tNode, const SqlNode_t & tLeft, const SqlNode_t & tRight )
{
	tNode.m_iParsedOp = m_dFilterTree.GetLength();

	FilterTreeItem_t & tElem = m_dFilterTree.Add();
	tElem.m_iLeft = tLeft.m_iParsedOp;
	tElem.m_iRight = tRight.m_iParsedOp;
}

void SqlParser_c::FilterOr ( SqlNode_t & tNode, const SqlNode_t & tLeft, const SqlNode_t & tRight )
{
	tNode.m_iParsedOp = m_dFilterTree.GetLength();
	m_bGotFilterOr = true;

	FilterTreeItem_t & tElem = m_dFilterTree.Add();
	tElem.m_bOr = true;
	tElem.m_iLeft = tLeft.m_iParsedOp;
	tElem.m_iRight = tRight.m_iParsedOp;
}


struct QueryItemProxy_t
{
	DWORD m_uHash;
	int m_iIndex;
	CSphQueryItem * m_pItem;

	bool operator < ( const QueryItemProxy_t & tItem ) const
	{
		return ( ( m_uHash<tItem.m_uHash ) || ( m_uHash==tItem.m_uHash && m_iIndex<tItem.m_iIndex ) );
	}

	bool operator == ( const QueryItemProxy_t & tItem ) const
	{
		return ( m_uHash==tItem.m_uHash );
	}

	void QueryItemHash ()
	{
		assert ( m_pItem );
		m_uHash = sphCRC32 ( m_pItem->m_sAlias.cstr() );
		m_uHash = sphCRC32 ( m_pItem->m_sExpr.cstr(), m_pItem->m_sExpr.Length(), m_uHash );
		m_uHash = sphCRC32 ( (const void*)&m_pItem->m_eAggrFunc, sizeof(m_pItem->m_eAggrFunc), m_uHash );
	}
};

static void CreateFilterTree ( const CSphVector<FilterTreeItem_t> & dOps, int iStart, int iCount, CSphQuery & tQuery )
{
	bool bHasOr = false;
	int iTreeCount = iCount - iStart;
	CSphVector<FilterTreeItem_t> dTree ( iTreeCount );
	for ( int i = 0; i<iTreeCount; i++ )
	{
		FilterTreeItem_t tItem = dOps[iStart + i];
		tItem.m_iLeft = ( tItem.m_iLeft==-1 ? -1 : tItem.m_iLeft - iStart );
		tItem.m_iRight = ( tItem.m_iRight==-1 ? -1 : tItem.m_iRight - iStart );
		dTree[i] = tItem;
		bHasOr |= ( tItem.m_iFilterItem==-1 && tItem.m_bOr );
	}

	// query has only plain AND filters - no need for filter tree
	if ( !bHasOr )
		return;

	tQuery.m_dFilterTree.SwapData ( dTree );
}


struct HintComp_fn
{
	bool IsLess ( const IndexHint_t & tA, const IndexHint_t & tB ) const
	{
		return strcasecmp ( tA.m_sIndex.cstr(), tB.m_sIndex.cstr() ) < 0;
	}

	bool IsEq ( const IndexHint_t & tA, const IndexHint_t & tB ) const
	{
		return tA.m_sIndex==tB.m_sIndex && tA.m_eHint==tB.m_eHint;
	}
};


static bool CheckQueryHints ( CSphVector<IndexHint_t> & dHints, CSphString & sError )
{
	sphSort ( dHints.Begin(), dHints.GetLength(), HintComp_fn() );
	sphUniq ( dHints.Begin(), dHints.GetLength(), HintComp_fn() );

	for ( int i = 1; i < dHints.GetLength(); i++ )
		if ( dHints[i-1].m_sIndex==dHints[i].m_sIndex )
		{
			sError.SetSprintf ( "conflicting hints specified for index '%s'", dHints[i-1].m_sIndex.cstr() );
			return false;
		}

	return true;
}


bool sphParseSqlQuery ( const char * sQuery, int iLen, CSphVector<SqlStmt_t> & dStmt, CSphString & sError, ESphCollation eCollation )
{
	if ( !sQuery || !iLen )
	{
		sError = "query was empty";
		return false;
	}

	// DDL is not supported in multi-statements anyway, so we only check the first statement
	if ( IsDdlQuery ( sQuery, iLen ) )
		return ParseDdl ( sQuery, iLen, dStmt, sError );

	SqlParser_c tParser ( dStmt, eCollation );
	tParser.m_pBuf = sQuery;
	tParser.m_pLastTokenStart = NULL;
	tParser.m_pParseError = &sError;
	tParser.m_eCollation = eCollation;

	char * sEnd = const_cast<char *>( sQuery ) + iLen;
	sEnd[0] = 0; // prepare for yy_scan_buffer
	sEnd[1] = 0; // this is ok because string allocates a small gap

	yylex_init ( &tParser.m_pScanner );
	YY_BUFFER_STATE tLexerBuffer = yy_scan_buffer ( const_cast<char *>( sQuery ), iLen+2, tParser.m_pScanner );
	if ( !tLexerBuffer )
	{
		sError = "internal error: yy_scan_buffer() failed";
		return false;
	}

	int iRes = yyparse ( &tParser );

	yy_delete_buffer ( tLexerBuffer, tParser.m_pScanner );
	yylex_destroy ( tParser.m_pScanner );

	dStmt.Pop(); // last query is always dummy

	int iFilterStart = 0;
	int iFilterCount = 0;
	ARRAY_FOREACH ( iStmt, dStmt )
	{
		// select expressions will be reparsed again, by an expression parser,
		// when we have an index to actually bind variables, and create a tree
		//
		// so at SQL parse stage, we only do quick validation, and at this point,
		// we just store the select list for later use by the expression parser
		CSphQuery & tQuery = dStmt[iStmt].m_tQuery;
		if ( tQuery.m_iSQLSelectStart>=0 )
		{
			if ( tQuery.m_iSQLSelectStart-1>=0 && tParser.m_pBuf[tQuery.m_iSQLSelectStart-1]=='`' )
				tQuery.m_iSQLSelectStart--;
			if ( tQuery.m_iSQLSelectEnd<iLen && tParser.m_pBuf[tQuery.m_iSQLSelectEnd]=='`' )
				tQuery.m_iSQLSelectEnd++;

			tQuery.m_sSelect.SetBinary ( tParser.m_pBuf + tQuery.m_iSQLSelectStart,
				tQuery.m_iSQLSelectEnd - tQuery.m_iSQLSelectStart );
		}

		// validate tablefuncs
		// tablefuncs are searchd-level builtins rather than common expression-level functions
		// so validation happens here, expression parser does not know tablefuncs (ignorance is bliss)
		if ( dStmt[iStmt].m_eStmt==STMT_SELECT && !dStmt[iStmt].m_sTableFunc.IsEmpty() )
		{
			CSphString & sFunc = dStmt[iStmt].m_sTableFunc;
			sFunc.ToUpper();

			ISphTableFunc * pFunc = NULL;
			if ( sFunc=="REMOVE_REPEATS" )
				pFunc = CreateRemoveRepeats();

			if ( !pFunc )
			{
				sError.SetSprintf ( "unknown table function %s()", sFunc.cstr() );
				return false;
			}
			if ( !pFunc->ValidateArgs ( dStmt[iStmt].m_dTableFuncArgs, tQuery, sError ) )
			{
				SafeDelete ( pFunc );
				return false;
			}
			dStmt[iStmt].m_pTableFunc = pFunc;
		}

		// validate filters
		ARRAY_FOREACH ( i, tQuery.m_dFilters )
		{
			const CSphString & sCol = tQuery.m_dFilters[i].m_sAttrName;
			if ( !strcasecmp ( sCol.cstr(), "@count" ) || !strcasecmp ( sCol.cstr(), "count(*)" ) )
			{
				sError.SetSprintf ( "sphinxql: aggregates in 'where' clause prohibited, use 'HAVING'" );
				return false;
			}
		}

		iFilterCount = tParser.m_dFiltersPerStmt[iStmt];
		// all queries have only plain AND filters - no need for filter tree
		if ( iFilterCount && tParser.m_bGotFilterOr )
			CreateFilterTree ( tParser.m_dFilterTree, iFilterStart, iFilterCount, tQuery );
		iFilterStart += iFilterCount;

		// fixup hints
		if ( !CheckQueryHints ( tQuery.m_dIndexHints, sError ) )
			return false;
	}

	if ( iRes!=0 || !dStmt.GetLength() )
		return false;

	if ( tParser.IsDeprecatedSyntax() )
	{
		sError = "Using the old-fashion @variables (@count, @weight, etc.) is deprecated";
		return false;
	}

	// facets
	bool bGotFacet = false;
	ARRAY_FOREACH ( i, dStmt )
	{
		const SqlStmt_t & tHeadStmt = dStmt[i];
		const CSphQuery & tHeadQuery = tHeadStmt.m_tQuery;
		if ( dStmt[i].m_eStmt==STMT_SELECT )
		{
			i++;
			if ( i<dStmt.GetLength() && dStmt[i].m_eStmt==STMT_FACET )
			{
				bGotFacet = true;
				const_cast<CSphQuery &>(tHeadQuery).m_bFacetHead = true;
			}

			for ( ; i<dStmt.GetLength() && dStmt[i].m_eStmt==STMT_FACET; i++ )
			{
				SqlStmt_t & tStmt = dStmt[i];
				tStmt.m_tQuery.m_bFacet = true;

				tStmt.m_eStmt = STMT_SELECT;
				tStmt.m_tQuery.m_sIndexes = tHeadQuery.m_sIndexes;
				tStmt.m_tQuery.m_sSelect = tStmt.m_tQuery.m_sFacetBy;
				tStmt.m_tQuery.m_sQuery = tHeadQuery.m_sQuery;
				tStmt.m_tQuery.m_iMaxMatches = tHeadQuery.m_iMaxMatches;

				// need to keep same wide result set schema
				tStmt.m_tQuery.m_sGroupDistinct = tHeadQuery.m_sGroupDistinct;

				// append filters
				ARRAY_FOREACH ( k, tHeadQuery.m_dFilters )
					tStmt.m_tQuery.m_dFilters.Add ( tHeadQuery.m_dFilters[k] );
				ARRAY_FOREACH ( k, tHeadQuery.m_dFilterTree )
					tStmt.m_tQuery.m_dFilterTree.Add ( tHeadQuery.m_dFilterTree[k] );
			}
		}
	}

	if ( bGotFacet )
	{
		// need to keep order of query items same as at select list however do not duplicate items
		// that is why raw Vector.Uniq does not work here
		CSphVector<QueryItemProxy_t> dSelectItems;
		ARRAY_FOREACH ( i, dStmt )
		{
			ARRAY_FOREACH ( k, dStmt[i].m_tQuery.m_dItems )
			{
				QueryItemProxy_t & tItem = dSelectItems.Add();
				tItem.m_pItem = dStmt[i].m_tQuery.m_dItems.Begin() + k;
				tItem.m_iIndex = dSelectItems.GetLength() - 1;
				tItem.QueryItemHash();
			}
		}
		// got rid of duplicates
		dSelectItems.Uniq();
		// sort back to select list appearance order
		dSelectItems.Sort ( bind ( &QueryItemProxy_t::m_iIndex ) );
		// get merged select list
		CSphVector<CSphQueryItem> dItems ( dSelectItems.GetLength() );
		ARRAY_FOREACH ( i, dSelectItems )
		{
			dItems[i] = *dSelectItems[i].m_pItem;
		}

		ARRAY_FOREACH ( i, dStmt )
		{
			SqlStmt_t & tStmt = dStmt[i];
			// keep original items
			tStmt.m_tQuery.m_dItems.SwapData ( dStmt[i].m_tQuery.m_dRefItems );
			tStmt.m_tQuery.m_dItems = dItems;

			// for FACET strip off group by expression items
			// these come after count(*)
			if ( tStmt.m_tQuery.m_bFacet )
			{
				ARRAY_FOREACH ( j, tStmt.m_tQuery.m_dRefItems )
				{
					if ( tStmt.m_tQuery.m_dRefItems[j].m_sAlias=="count(*)" )
					{
						tStmt.m_tQuery.m_dRefItems.Resize ( j+1 );
						break;
					}
				}
			}
		}
	}

	return true;
}

void SqlParser_c::SetIndex ( const SqlNode_t & tIndex, CSphString & sIndex ) const
{
	ToString ( sIndex, tIndex );
	SplitClusterIndex ( sIndex, nullptr );
}

void SqlParser_c::SetIndex ( const SqlNode_t & tIndex )
{
	assert ( m_pStmt );
	ToString ( m_pStmt->m_sIndex, tIndex );
	SplitClusterIndex ( m_pStmt->m_sIndex, &m_pStmt->m_sCluster );
}

void SqlParser_c::SplitClusterIndex ( CSphString & sIndex, CSphString * pCluster )
{
	if ( sIndex.IsEmpty() )
		return;

	const char * sDelimiter = strchr ( sIndex.cstr(), ':' );
	if ( sDelimiter )
	{
		CSphString sTmp = sIndex; // m_sIndex.SetBinary can not accept this(m_sIndex) pointer

		int iPos = sDelimiter - sIndex.cstr();
		int iLen = sIndex.Length();
		sIndex.SetBinary ( sTmp.cstr() + iPos + 1, iLen - iPos - 1 );
		if ( pCluster )
			pCluster->SetBinary ( sTmp.cstr(), iPos );
	}
}


//////////////////////////////////////////////////////////////////////////

bool PercolateParseFilters ( const char * sFilters, ESphCollation eCollation, const CSphSchema & tSchema, CSphVector<CSphFilterSettings> & dFilters, CSphVector<FilterTreeItem_t> & dFilterTree, CSphString & sError )
{
	if ( !sFilters || !*sFilters )
		return true;

	StringBuilder_c sBuf;
	sBuf << "sysfilters " << sFilters;
	int iLen = sBuf.GetLength();

	CSphVector<SqlStmt_t> dStmt;
	SqlParser_c tParser ( dStmt, eCollation );
	tParser.m_pBuf = sBuf.cstr();
	tParser.m_pLastTokenStart = nullptr;
	tParser.m_pParseError = &sError;
	tParser.m_eCollation = eCollation;
	tParser.m_sErrorHeader = "percolate filters:";

	char * sEnd = const_cast<char *>( sBuf.cstr() ) + iLen;
	sEnd[0] = 0; // prepare for yy_scan_buffer
	sEnd[1] = 0; // this is ok because string allocates a small gap

	yylex_init ( &tParser.m_pScanner );
	YY_BUFFER_STATE tLexerBuffer = yy_scan_buffer ( const_cast<char *>( sBuf.cstr() ), iLen+2, tParser.m_pScanner );
	if ( !tLexerBuffer )
	{
		sError = "internal error: yy_scan_buffer() failed";
		return false;
	}

	int iRes = yyparse ( &tParser );
	yy_delete_buffer ( tLexerBuffer, tParser.m_pScanner );
	yylex_destroy ( tParser.m_pScanner );

	dStmt.Pop(); // last query is always dummy

	if ( dStmt.GetLength()>1 )
	{
		sError.SetSprintf ( "internal error: too many filter statements, got %d", dStmt.GetLength() );
		return false;
	}

	if ( dStmt.GetLength() && dStmt[0].m_eStmt!=STMT_SYSFILTERS )
	{
		sError.SetSprintf ( "internal error: not filter statement parsed, got %d", dStmt[0].m_eStmt );
		return false;
	}

	if ( dStmt.GetLength() )
	{
		CSphQuery & tQuery = dStmt[0].m_tQuery;

		int iFilterCount = tParser.m_dFiltersPerStmt[0];
		CreateFilterTree ( tParser.m_dFilterTree, 0, iFilterCount, tQuery );

		dFilters.SwapData ( tQuery.m_dFilters );
		dFilterTree.SwapData ( tQuery.m_dFilterTree );
	}

	// maybe its better to create real filter instead of just checking column name
	if ( iRes==0 && dFilters.GetLength() )
	{
		ARRAY_FOREACH ( i, dFilters )
		{
			const CSphFilterSettings & tFilter = dFilters[i];
			if ( tFilter.m_sAttrName.IsEmpty() )
			{
				sError.SetSprintf ( "bad filter %d name", i );
				return false;
			}

			if ( tFilter.m_sAttrName.Begins ( "@" ) )
			{
				sError.SetSprintf ( "unsupported filter column '%s'", tFilter.m_sAttrName.cstr() );
				return false;
			}

			const char * sAttrName = tFilter.m_sAttrName.cstr();

			// might be a JSON.field
			CSphString sJsonField;
			const char * sJsonDot = strchr ( sAttrName, '.' );
			if ( sJsonDot )
			{
				assert ( sJsonDot>sAttrName );
				sJsonField.SetBinary ( sAttrName, sJsonDot - sAttrName );
				sAttrName = sJsonField.cstr();
			}

			int iCol = tSchema.GetAttrIndex ( sAttrName );
			if ( iCol==-1 )
			{
				sError.SetSprintf ( "no such filter attribute '%s'", sAttrName );
				return false;
			}
		}
	}

	// TODO: change way of filter -> expression create: produce single error, share parser code
	// try expression
	if ( iRes!=0 && !dFilters.GetLength() && sError.Begins ( "percolate filters: syntax error" ) )
	{
		ESphAttr eAttrType = SPH_ATTR_NONE;
		ExprParseArgs_t tExprArgs;
		tExprArgs.m_pAttrType = &eAttrType;
		tExprArgs.m_eCollation = eCollation;
		ISphExprRefPtr_c pExpr { sphExprParse ( sFilters, tSchema, sError, tExprArgs ) };
		if ( pExpr )
		{
			sError = "";
			iRes = 0;
			CSphFilterSettings & tExpr = dFilters.Add();
			tExpr.m_eType = SPH_FILTER_EXPRESSION;
			tExpr.m_sAttrName = sFilters;
		} else
		{
			return false;
		}
	}

	return ( iRes==0 );
}


int sphGetTokTypeInt()
{
	return TOK_CONST_INT;
}


int sphGetTokTypeFloat()
{
	return TOK_CONST_FLOAT;
}


int sphGetTokTypeStr()
{
	return TOK_QUOTED_STRING;
}

int sphGetTokTypeConstMVA()
{
	return TOK_CONST_MVA;
}