#include "global.h"
#include "PrefsManager.h"
#include "RageLog.h"
#include "song.h"
#include "Course.h"
#include "RageUtil.h"
#include "UnlockManager.h"
#include "SongManager.h"
#include "GameState.h"
#include "ProfileManager.h"
#include "ThemeManager.h"

UnlockManager*	UNLOCKMAN = NULL;	// global and accessable from anywhere in our program

#define UNLOCK_NAMES					THEME->GetMetric ("Unlocks","UnlockNames")
#define UNLOCK(sLineName)				THEME->GetMetric ("Unlocks",ssprintf("Unlock%s",sLineName.c_str()))

static CString UnlockTypeNames[NUM_UNLOCK_TYPES] =
{
	"ArcadePoints",
	"DancePoints",
	"SongPoints",
	"ExtraCleared",
	"ExtraFailed",
	"Toasties",
	"StagesCleared"
};
XToString(UnlockType);
StringToX(UnlockType);

UnlockManager::UnlockManager()
{
	UNLOCKMAN = this;

	Load();
}

void UnlockManager::UnlockSong( const Song *song )
{
	const UnlockEntry *p = FindSong( song );
	if( !p )
		return;  // does not exist
	if( p->m_iCode == -1 )
		return;

	UnlockCode( p->m_iCode );
}

bool UnlockManager::CourseIsLocked( const Course *course ) const
{
	if( !PREFSMAN->m_bUseUnlockSystem )
		return false;

	const UnlockEntry *p = FindCourse( course );
	if( p == NULL )
		return false;

	return p->IsLocked();
}

bool UnlockManager::SongIsLocked( const Song *song ) const
{
	if( !PREFSMAN->m_bUseUnlockSystem )
		return false;

	const UnlockEntry *p = FindSong( song );
	if( p == NULL )
		return false;

	return p->IsLocked();
}

/* Return true if the song is *only* available in roulette. */
bool UnlockManager::SongIsRouletteOnly( const Song *song ) const
{
	if( !PREFSMAN->m_bUseUnlockSystem )
		return false;

	const UnlockEntry *p = FindSong( song );
	if( !p )
		return false;

	/* If the song is locked by a code, and it's a roulette code, honor IsLocked. */
	if( p->m_iCode == -1 || m_RouletteCodes.find( p->m_iCode ) == m_RouletteCodes.end() )
		return false;

	return p->IsLocked();
}

const UnlockEntry *UnlockManager::FindLockEntry( CString songname ) const
{
	for( unsigned i = 0; i < m_SongEntries.size(); i++ )
		if( !songname.CompareNoCase(m_SongEntries[i].m_sSongName) )
			return &m_SongEntries[i];

	return NULL;
}

const UnlockEntry *UnlockManager::FindSong( const Song *pSong ) const
{
	for( unsigned i = 0; i < m_SongEntries.size(); i++ )
		if( m_SongEntries[i].m_pSong == pSong )
			return &m_SongEntries[i];

	return NULL;
}

const UnlockEntry *UnlockManager::FindCourse( const Course *pCourse ) const
{
	for(unsigned i = 0; i < m_SongEntries.size(); i++)
		if (m_SongEntries[i].m_pCourse== pCourse )
			return &m_SongEntries[i];

	return NULL;
}


UnlockEntry::UnlockEntry()
{
	memset( m_fRequired, 0, sizeof(m_fRequired) );
	m_iCode = -1;

	m_pSong = NULL;
	m_pCourse = NULL;
}

static float GetArcadePoints( const Profile *pProfile )
{
	float fAP =	0;

	FOREACH_Grade(g)
	{
		switch(g)
		{
		case GRADE_TIER_1:
		case GRADE_TIER_2:	fAP += 9 * pProfile->m_iNumStagesPassedByGrade[g]; break;
		default:			fAP += 1 * pProfile->m_iNumStagesPassedByGrade[g]; break;

		case GRADE_FAILED:
		case GRADE_NO_DATA:
			;	// no points
			break;
		}
	}

	FOREACH_PlayMode(pm)
	{
		switch(pm)
		{
		case PLAY_MODE_NONSTOP:
		case PLAY_MODE_ONI:
		case PLAY_MODE_ENDLESS:
			fAP += pProfile->m_iNumSongsPlayedByPlayMode[pm];
			break;
		}

	}

	return fAP;
}

static float GetSongPoints( const Profile *pProfile )
{
	float fSP =	0;

	FOREACH_Grade(g)
	{
		switch( g )
		{
		case GRADE_TIER_1:/*AAAA*/	fSP += 20 * pProfile->m_iNumStagesPassedByGrade[g];	break;
		case GRADE_TIER_2:/*AAA*/	fSP += 10* pProfile->m_iNumStagesPassedByGrade[g];	break;
		case GRADE_TIER_3:/*AA*/	fSP += 5* pProfile->m_iNumStagesPassedByGrade[g];	break;
		case GRADE_TIER_4:/*A*/		fSP += 4* pProfile->m_iNumStagesPassedByGrade[g];	break;
		case GRADE_TIER_5:/*B*/		fSP += 3* pProfile->m_iNumStagesPassedByGrade[g];	break;
		case GRADE_TIER_6:/*C*/		fSP += 2* pProfile->m_iNumStagesPassedByGrade[g];	break;
		case GRADE_TIER_7:/*D*/		fSP += 1* pProfile->m_iNumStagesPassedByGrade[g];	break;
		case GRADE_FAILED:
		case GRADE_NO_DATA:
			;	// no points
			break;
		}
	}

	FOREACH_PlayMode(pm)
	{
		switch(pm)
		{
		case PLAY_MODE_NONSTOP:
		case PLAY_MODE_ONI:
		case PLAY_MODE_ENDLESS:
			fSP += pProfile->m_iNumSongsPlayedByPlayMode[pm];
			break;
		}

	}

	return fSP;
}

void UnlockManager::GetPoints( const Profile *pProfile, float fScores[NUM_UNLOCK_TYPES] ) const
{
	fScores[UNLOCK_ARCADE_POINTS] = GetArcadePoints( pProfile );
	fScores[UNLOCK_SONG_POINTS] = GetSongPoints( pProfile );
	fScores[UNLOCK_DANCE_POINTS] = (float) pProfile->m_iTotalDancePoints;
	fScores[UNLOCK_CLEARED] = (float) pProfile->GetTotalNumSongsPassed();
}

bool UnlockEntry::IsLocked() const
{
	float fScores[NUM_UNLOCK_TYPES];
	UNLOCKMAN->GetPoints( PROFILEMAN->GetMachineProfile(), fScores );

	for( int i = 0; i < NUM_UNLOCK_TYPES; ++i )
		if( m_fRequired[i] && fScores[i] >= m_fRequired[i] )
			return false;

	if( m_iCode != -1 && PROFILEMAN->GetMachineProfile()->m_UnlockedSongs.find(m_iCode) != PROFILEMAN->GetMachineProfile()->m_UnlockedSongs.end() )
		return false;

	return true;
}

void UnlockManager::Load()
{
	LOG->Trace( "UnlockManager::Load()" );

	CStringArray asUnlockNames;
	split( UNLOCK_NAMES, ",", asUnlockNames );
	if( asUnlockNames.empty() )
		return;

	for( unsigned i = 0; i < asUnlockNames.size(); ++i )
	{
		const CString &sUnlockName = asUnlockNames[i];
		CString sUnlock = UNLOCK(sUnlockName);

		Commands vCommands;
		ParseCommands( sUnlock, vCommands );

		UnlockEntry current;
		bool bRoulette = false;

		for( unsigned j = 0; j < vCommands.v.size(); ++j )
		{
			const Command &cmd = vCommands.v[j];
			if( cmd.GetName() == "song" )
				current.m_sSongName = (CString) cmd.GetArg(1);
			else if( cmd.GetName() == "code" )
			{
				// Hack: Lua only has a floating point type, and codes may be big enough
				// that converting them from string to float to int introduces rounding
				// error.  Convert directly to int.
				current.m_iCode = atoi( (CString) cmd.GetArg(1) );
			}
			else if( cmd.GetName() == "roulette" )
				bRoulette = true;
			else
			{
				const UnlockType ut = StringToUnlockType( cmd.GetName() );
				if( ut != UNLOCK_INVALID )
					current.m_fRequired[ut] = cmd.GetArg(1);
			}
		}

		if( bRoulette )
			m_RouletteCodes.insert( current.m_iCode );

		m_SongEntries.push_back( current );
	}

	UpdateSongs();

	for( unsigned i=0; i < m_SongEntries.size(); i++ )
	{
		CString str = ssprintf( "Unlock: %s; ", m_SongEntries[i].m_sSongName.c_str() );
		FOREACH_UnlockType(j)
			if( m_SongEntries[i].m_fRequired[j] )
				str += ssprintf( "%s = %f; ", UnlockTypeToString(j).c_str(), m_SongEntries[i].m_fRequired[j] );

		str += ssprintf( "code = %i ", m_SongEntries[i].m_iCode );
		str += m_SongEntries[i].IsLocked()? "locked":"unlocked";
		if( m_SongEntries[i].m_pSong )
			str += ( " (found song)" );
		if( m_SongEntries[i].m_pCourse )
			str += ( " (found course)" );
		LOG->Trace( "%s", str.c_str() );
	}
	
	return;
}

float UnlockManager::PointsUntilNextUnlock( UnlockType t ) const
{
	float fScores[NUM_UNLOCK_TYPES];
	UNLOCKMAN->GetPoints( PROFILEMAN->GetMachineProfile(), fScores );

	float fSmallestPoints = 400000000;   // or an arbitrarily large value
	for( unsigned a=0; a<m_SongEntries.size(); a++ )
		if( m_SongEntries[a].m_fRequired[t] > fScores[t] )
			fSmallestPoints = min( fSmallestPoints, m_SongEntries[a].m_fRequired[t] );
	
	if( fSmallestPoints == 400000000 )
		return 0;  // no match found
	return fSmallestPoints - fScores[t];
}

/* Update the song pointer.  Only call this when it's likely to have changed,
 * such as on load, or when a song title changes in the editor. */
void UnlockManager::UpdateSongs()
{
	for( unsigned i = 0; i < m_SongEntries.size(); ++i )
	{
		m_SongEntries[i].m_pSong = NULL;
		m_SongEntries[i].m_pCourse = NULL;
		if( m_SongEntries[i].m_sSongName != "" )
			m_SongEntries[i].m_pSong = SONGMAN->FindSong( m_SongEntries[i].m_sSongName );
        if( m_SongEntries[i].m_pSong == NULL )
                m_SongEntries[i].m_pCourse = SONGMAN->FindCourse( m_SongEntries[i].m_sSongName );

		// display warning on invalid song entry
		if( m_SongEntries[i].m_pSong == NULL && m_SongEntries[i].m_pCourse == NULL )
		{
			LOG->Warn( "Unlock: Cannot find a matching entry for \"%s\"", m_SongEntries[i].m_sSongName.c_str() );
			m_SongEntries.erase( m_SongEntries.begin() + i );
			--i;
		}
	}
}



void UnlockManager::UnlockCode( int num )
{
	FOREACH_PlayerNumber( pn )
		if( PROFILEMAN->IsUsingProfile(pn) )
			PROFILEMAN->GetProfile(pn)->m_UnlockedSongs.insert( num );

	PROFILEMAN->GetMachineProfile()->m_UnlockedSongs.insert( num );
}

void UnlockManager::PreferUnlockCode( int iCode )
{
	for( unsigned i = 0; i < m_SongEntries.size(); ++i )
	{
		UnlockEntry &pEntry = m_SongEntries[i];
		if( pEntry.m_iCode != iCode )
			continue;

		if( pEntry.m_pSong != NULL )
			GAMESTATE->m_pPreferredSong = pEntry.m_pSong;
		if( pEntry.m_pCourse != NULL )
			GAMESTATE->m_pPreferredCourse = pEntry.m_pCourse;
	}
}

int UnlockManager::GetNumUnlocks() const
{
	return m_SongEntries.size();
}

#include "LuaBinding.h"

template<class T>
class LunaUnlockManager: public Luna<T>
{
public:
	LunaUnlockManager() { LUA->Register( Register ); }

	static int UnlockCode( T* p, lua_State *L )			{ int iCode = IArg(1); p->UnlockCode(iCode); return 0; }
	static int PreferUnlockCode( T* p, lua_State *L )	{ int iCode = IArg(1); p->PreferUnlockCode(iCode); return 0; }

	static void Register(lua_State *L)
	{
		ADD_METHOD( UnlockCode )
		ADD_METHOD( PreferUnlockCode )
		Luna<T>::Register( L );

		// Add global singleton if constructed already.  If it's not constructed yet,
		// then we'll register it later when we reinit Lua just before 
		// initializing the display.
		if( UNLOCKMAN )
		{
			lua_pushstring(L, "UNLOCKMAN");
			UNLOCKMAN->PushSelf( LUA->L );
			lua_settable(L, LUA_GLOBALSINDEX);
		}
	}
};

LUA_REGISTER_CLASS( UnlockManager )

/*
 * (c) 2001-2004 Kevin Slaughter, Andrew Wong, Glenn Maynard
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
