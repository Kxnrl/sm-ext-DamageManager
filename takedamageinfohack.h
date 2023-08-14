#ifndef TAKEDAMAGEINFOHACK_H
#define TAKEDAMAGEINFOHACK_H
#ifdef _WIN32
#    pragma once
#endif

#include "smsdk_ext.h"

#define GAME_DLL 1

#include <isaverestore.h>

#ifndef _DEBUG
#    include <ehandle.h>
#else
#    undef _DEBUG
#    include <ehandle.h>
#    define _DEBUG 1
#endif

#include <server_class.h>

#include <shareddefs.h>
#include <takedamageinfo.h>

class CTakeDamageInfoHack : public CTakeDamageInfo
{
public:
    CTakeDamageInfoHack(CBaseEntity* pInflictor, CBaseEntity* pAttacker, float flDamage, int bitsDamageType,
                        CBaseEntity* pWeapon, Vector vecDamageForce, Vector vecDamagePosition);
    int  GetAttacker() const;
    void SetAttacker(CBaseEntity* pAttacker);

    inline int GetInflictor() const
    {
        return m_hInflictor.IsValid() ? m_hInflictor.GetEntryIndex() : -1;
    }
#if SOURCE_ENGINE >= SE_ORANGEBOX && SOURCE_ENGINE != SE_LEFT4DEAD
    inline int GetWeapon() const
    {
        return m_hWeapon.IsValid() ? m_hWeapon.GetEntryIndex() : -1;
    }
#else
    inline int GetWeapon() const
    {
        return -1;
    }
    inline void SetWeapon(CBaseEntity*)
    {}
#endif

    inline void SetDamageForce(vec_t x, vec_t y, vec_t z)
    {
        m_vecDamageForce.x = x;
        m_vecDamageForce.y = y;
        m_vecDamageForce.z = z;
    }

    inline void SetDamagePosition(vec_t x, vec_t y, vec_t z)
    {
        m_vecDamagePosition.x = x;
        m_vecDamagePosition.y = y;
        m_vecDamagePosition.z = z;
    }
#if SOURCE_ENGINE < SE_ORANGEBOX
    inline int GetDamageCustom() const
    {
        return GetCustomKill();
    }
#endif
};

#endif // TAKEDAMAGEINFOHACK_H
