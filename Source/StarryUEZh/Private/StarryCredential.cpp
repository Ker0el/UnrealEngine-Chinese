// Copyright 2026 StarGame. All Rights Reserved.

#include "StarryCredential.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <wincred.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
	const TCHAR* CredentialTarget = TEXT("StarryUEZh/DeepSeek");
	const TCHAR* CredentialUser = TEXT("api");
}

bool FStarryCredential::SaveApiKey(const FString& Key)
{
#if PLATFORM_WINDOWS
	if (Key.IsEmpty())
	{
		return false;
	}

	FTCHARToUTF8 Utf8(*Key);
	FString Target(CredentialTarget);
	FString User(CredentialUser);

	CREDENTIALW Credential = {};
	Credential.Type = CRED_TYPE_GENERIC;
	Credential.TargetName = const_cast<LPWSTR>(*Target);
	Credential.UserName = const_cast<LPWSTR>(*User);
	Credential.CredentialBlobSize = static_cast<DWORD>(Utf8.Length());
	Credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<ANSICHAR*>(Utf8.Get()));
	Credential.Persist = CRED_PERSIST_LOCAL_MACHINE;

	return ::CredWriteW(&Credential, 0) != 0;
#else
	return false;
#endif
}

bool FStarryCredential::LoadApiKey(FString& OutKey)
{
#if PLATFORM_WINDOWS
	FString Target(CredentialTarget);
	PCREDENTIALW Credential = nullptr;
	if (!::CredReadW(*Target, CRED_TYPE_GENERIC, 0, &Credential) || !Credential)
	{
		return false;
	}

	const FUTF8ToTCHAR Converter(
		reinterpret_cast<const ANSICHAR*>(Credential->CredentialBlob),
		static_cast<int32>(Credential->CredentialBlobSize));
	OutKey = FString(Converter.Length(), Converter.Get());
	::CredFree(Credential);
	return !OutKey.IsEmpty();
#else
	return false;
#endif
}

void FStarryCredential::DeleteApiKey()
{
#if PLATFORM_WINDOWS
	FString Target(CredentialTarget);
	::CredDeleteW(*Target, CRED_TYPE_GENERIC, 0);
#endif
}
