/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAppFileLocationProvider.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsEnumeratorUtils.h"
#include "nsIAtom.h"
#include "nsIFile.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsISimpleEnumerator.h"
#include "prenv.h"
#include "nsCRT.h"
#include "nsXPCOMPrivate.h"  // for XPCOM_FILE_PATH_SEPARATOR

#if defined(MOZ_WIDGET_COCOA)
#include <Carbon/Carbon.h>
#include "nsILocalFileMac.h"
#elif defined(XP_WIN)
#include <windows.h>
#include <shlobj.h>
#elif defined(XP_UNIX)
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#endif

#include "TorFileUtils.h"

// WARNING: These hard coded names need to go away. They need to
// come from localizable resources

#if defined(MOZ_WIDGET_COCOA)
#define APP_REGISTRY_NAME NS_LITERAL_CSTRING("Application Registry")
#define ESSENTIAL_FILES   NS_LITERAL_CSTRING("Essential Files")
#elif defined(XP_WIN)
#define APP_REGISTRY_NAME NS_LITERAL_CSTRING("registry.dat")
#else
#define APP_REGISTRY_NAME NS_LITERAL_CSTRING("appreg")
#endif

// define default product directory
#define DEFAULT_PRODUCT_DIR NS_LITERAL_CSTRING(MOZ_USER_DIR)

// Locally defined keys used by nsAppDirectoryEnumerator
#define NS_ENV_PLUGINS_DIR          "EnvPlugins"    // env var MOZ_PLUGIN_PATH
#define NS_USER_PLUGINS_DIR         "UserPlugins"

#ifdef MOZ_WIDGET_COCOA
#define NS_MACOSX_USER_PLUGIN_DIR   "OSXUserPlugins"
#define NS_MACOSX_LOCAL_PLUGIN_DIR  "OSXLocalPlugins"
#define NS_MACOSX_JAVA2_PLUGIN_DIR  "OSXJavaPlugins"
#elif XP_UNIX
#define NS_SYSTEM_PLUGINS_DIR       "SysPlugins"
#endif

#define DEFAULTS_DIR_NAME           NS_LITERAL_CSTRING("defaults")
#define DEFAULTS_PREF_DIR_NAME      NS_LITERAL_CSTRING("pref")
#define RES_DIR_NAME                NS_LITERAL_CSTRING("res")
#define CHROME_DIR_NAME             NS_LITERAL_CSTRING("chrome")
#define PLUGINS_DIR_NAME            NS_LITERAL_CSTRING("plugins")
#define SEARCH_DIR_NAME             NS_LITERAL_CSTRING("searchplugins")

//*****************************************************************************
// nsAppFileLocationProvider::Constructor/Destructor
//*****************************************************************************

nsAppFileLocationProvider::nsAppFileLocationProvider()
{
}

//*****************************************************************************
// nsAppFileLocationProvider::nsISupports
//*****************************************************************************

NS_IMPL_ISUPPORTS(nsAppFileLocationProvider,
                  nsIDirectoryServiceProvider,
                  nsIDirectoryServiceProvider2)

//*****************************************************************************
// nsAppFileLocationProvider::nsIDirectoryServiceProvider
//*****************************************************************************

NS_IMETHODIMP
nsAppFileLocationProvider::GetFile(const char* aProp, bool* aPersistent,
                                   nsIFile** aResult)
{
  if (NS_WARN_IF(!aProp)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIFile>  localFile;
  nsresult rv = NS_ERROR_FAILURE;

  *aResult = nullptr;
  *aPersistent = true;

#ifdef MOZ_WIDGET_COCOA
  FSRef fileRef;
  nsCOMPtr<nsILocalFileMac> macFile;
#endif

  if (nsCRT::strcmp(aProp, NS_APP_APPLICATION_REGISTRY_DIR) == 0) {
    rv = GetProductDirectory(getter_AddRefs(localFile));
  } else if (nsCRT::strcmp(aProp, NS_APP_APPLICATION_REGISTRY_FILE) == 0) {
    rv = GetProductDirectory(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv)) {
      rv = localFile->AppendNative(APP_REGISTRY_NAME);
    }
  } else if (nsCRT::strcmp(aProp, NS_APP_DEFAULTS_50_DIR) == 0) {
    rv = CloneMozBinDirectory(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv)) {
      rv = localFile->AppendRelativeNativePath(DEFAULTS_DIR_NAME);
    }
  } else if (nsCRT::strcmp(aProp, NS_APP_PREF_DEFAULTS_50_DIR) == 0) {
    rv = CloneMozBinDirectory(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv)) {
      rv = localFile->AppendRelativeNativePath(DEFAULTS_DIR_NAME);
      if (NS_SUCCEEDED(rv)) {
        rv = localFile->AppendRelativeNativePath(DEFAULTS_PREF_DIR_NAME);
      }
    }
  } else if (nsCRT::strcmp(aProp, NS_APP_USER_PROFILES_ROOT_DIR) == 0) {
    rv = GetDefaultUserProfileRoot(getter_AddRefs(localFile));
  } else if (nsCRT::strcmp(aProp, NS_APP_USER_PROFILES_LOCAL_ROOT_DIR) == 0) {
    rv = GetDefaultUserProfileRoot(getter_AddRefs(localFile), true);
  } else if (nsCRT::strcmp(aProp, NS_APP_RES_DIR) == 0) {
    rv = CloneMozBinDirectory(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv)) {
      rv = localFile->AppendRelativeNativePath(RES_DIR_NAME);
    }
  } else if (nsCRT::strcmp(aProp, NS_APP_CHROME_DIR) == 0) {
    rv = CloneMozBinDirectory(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv)) {
      rv = localFile->AppendRelativeNativePath(CHROME_DIR_NAME);
    }
  } else if (nsCRT::strcmp(aProp, NS_APP_PLUGINS_DIR) == 0) {
    rv = CloneMozBinDirectory(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv)) {
      rv = localFile->AppendRelativeNativePath(PLUGINS_DIR_NAME);
    }
  }
#ifdef MOZ_WIDGET_COCOA
  else if (nsCRT::strcmp(aProp, NS_MACOSX_USER_PLUGIN_DIR) == 0) {
    if (::FSFindFolder(kUserDomain, kInternetPlugInFolderType, false,
                       &fileRef) == noErr) {
      rv = NS_NewLocalFileWithFSRef(&fileRef, true, getter_AddRefs(macFile));
      if (NS_SUCCEEDED(rv)) {
        localFile = macFile;
      }
    }
  } else if (nsCRT::strcmp(aProp, NS_MACOSX_LOCAL_PLUGIN_DIR) == 0) {
    if (::FSFindFolder(kLocalDomain, kInternetPlugInFolderType, false,
                       &fileRef) == noErr) {
      rv = NS_NewLocalFileWithFSRef(&fileRef, true, getter_AddRefs(macFile));
      if (NS_SUCCEEDED(rv)) {
        localFile = macFile;
      }
    }
  } else if (nsCRT::strcmp(aProp, NS_MACOSX_JAVA2_PLUGIN_DIR) == 0) {
    static const char* const java2PluginDirPath =
      "/System/Library/Java/Support/Deploy.bundle/Contents/Resources/";
    rv = NS_NewNativeLocalFile(nsDependentCString(java2PluginDirPath), true,
                               getter_AddRefs(localFile));
  }
#else
  else if (nsCRT::strcmp(aProp, NS_ENV_PLUGINS_DIR) == 0) {
    NS_ERROR("Don't use nsAppFileLocationProvider::GetFile(NS_ENV_PLUGINS_DIR, ...). "
             "Use nsAppFileLocationProvider::GetFiles(...).");
    const char* pathVar = PR_GetEnv("MOZ_PLUGIN_PATH");
    if (pathVar && *pathVar)
      rv = NS_NewNativeLocalFile(nsDependentCString(pathVar), true,
                                 getter_AddRefs(localFile));
  } else if (nsCRT::strcmp(aProp, NS_USER_PLUGINS_DIR) == 0) {
#ifdef ENABLE_SYSTEM_EXTENSION_DIRS
    rv = GetProductDirectory(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv)) {
      rv = localFile->AppendRelativeNativePath(PLUGINS_DIR_NAME);
    }
#else
    rv = NS_ERROR_FAILURE;
#endif
  }
#ifdef XP_UNIX
  else if (nsCRT::strcmp(aProp, NS_SYSTEM_PLUGINS_DIR) == 0) {
#ifdef ENABLE_SYSTEM_EXTENSION_DIRS
    static const char* const sysLPlgDir =
#if defined(HAVE_USR_LIB64_DIR) && defined(__LP64__)
      "/usr/lib64/mozilla/plugins";
#elif defined(__OpenBSD__) || defined (__FreeBSD__)
      "/usr/local/lib/mozilla/plugins";
#else
      "/usr/lib/mozilla/plugins";
#endif
    rv = NS_NewNativeLocalFile(nsDependentCString(sysLPlgDir),
                               false, getter_AddRefs(localFile));
#else
    rv = NS_ERROR_FAILURE;
#endif
  }
#endif
#endif
  else if (nsCRT::strcmp(aProp, NS_APP_SEARCH_DIR) == 0) {
    rv = CloneMozBinDirectory(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv)) {
      rv = localFile->AppendRelativeNativePath(SEARCH_DIR_NAME);
    }
  } else if (nsCRT::strcmp(aProp, NS_APP_USER_SEARCH_DIR) == 0) {
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, aResult);
    if (NS_SUCCEEDED(rv)) {
      rv = (*aResult)->AppendNative(SEARCH_DIR_NAME);
    }
  } else if (nsCRT::strcmp(aProp, NS_APP_INSTALL_CLEANUP_DIR) == 0) {
    // This is cloned so that embeddors will have a hook to override
    // with their own cleanup dir.  See bugzilla bug #105087
    rv = CloneMozBinDirectory(getter_AddRefs(localFile));
  }

  if (localFile && NS_SUCCEEDED(rv)) {
    localFile.forget(aResult);
    return NS_OK;
  }

  return rv;
}


nsresult
nsAppFileLocationProvider::CloneMozBinDirectory(nsIFile** aLocalFile)
{
  if (NS_WARN_IF(!aLocalFile)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsresult rv;

  if (!mMozBinDirectory) {
    // Get the mozilla bin directory
    // 1. Check the directory service first for NS_XPCOM_CURRENT_PROCESS_DIR
    //    This will be set if a directory was passed to NS_InitXPCOM
    // 2. If that doesn't work, set it to be the current process directory
    nsCOMPtr<nsIProperties>
    directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = directoryService->Get(NS_XPCOM_CURRENT_PROCESS_DIR, NS_GET_IID(nsIFile),
                               getter_AddRefs(mMozBinDirectory));
    if (NS_FAILED(rv)) {
      rv = directoryService->Get(NS_OS_CURRENT_PROCESS_DIR, NS_GET_IID(nsIFile),
                                 getter_AddRefs(mMozBinDirectory));
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  nsCOMPtr<nsIFile> aFile;
  rv = mMozBinDirectory->Clone(getter_AddRefs(aFile));
  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_IF_ADDREF(*aLocalFile = aFile);
  return NS_OK;
}


//----------------------------------------------------------------------------------------
// GetProductDirectory - Gets the directory which contains the application data folder
//
#ifdef TOR_BROWSER_DATA_OUTSIDE_APP_DIR
// UNIX and WIN   : <App Folder>/../TorBrowser-Data/Browser
// Mac            : <App Folder>/../../../TorBrowser-Data/Browser OR
//                  ~/Library/Application Support/TorBrowser-Data/Browser
#else
// UNIX and WIN   : <App Folder>/TorBrowser/Data/Browser
// Mac            : <App Folder>/../../TorBrowser/Data/Browser
#endif
//----------------------------------------------------------------------------------------
nsresult
nsAppFileLocationProvider::GetProductDirectory(nsIFile** aLocalFile,
                                               bool aLocal)
{
  if (NS_WARN_IF(!aLocalFile)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv = NS_ERROR_UNEXPECTED;
  bool exists;
  nsCOMPtr<nsIFile> localDir, exeFile;

  nsCOMPtr<nsIProperties> directoryService(
                         do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = directoryService->Get(XRE_EXECUTABLE_FILE, NS_GET_IID(nsIFile),
                             getter_AddRefs(exeFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = TorBrowser_GetUserDataDir(exeFile, getter_AddRefs(localDir));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef TOR_BROWSER_DATA_OUTSIDE_APP_DIR
  rv = localDir->AppendNative(NS_LITERAL_CSTRING("Browser"));
#else
  rv = localDir->AppendRelativeNativePath(NS_LITERAL_CSTRING("Data"
                                        XPCOM_FILE_PATH_SEPARATOR "Browser"));
#endif
  NS_ENSURE_SUCCESS(rv, rv);

  if (aLocal) {
    rv = localDir->AppendNative(NS_LITERAL_CSTRING("Caches"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = localDir->Exists(&exists);

  if (NS_SUCCEEDED(rv) && !exists) {
    rv = localDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  localDir.forget(aLocalFile);

  return rv;
}


//----------------------------------------------------------------------------------------
// GetDefaultUserProfileRoot - Gets the directory which contains each user profile dir
//----------------------------------------------------------------------------------------
nsresult
nsAppFileLocationProvider::GetDefaultUserProfileRoot(nsIFile** aLocalFile,
                                                     bool aLocal)
{
  if (NS_WARN_IF(!aLocalFile)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  nsCOMPtr<nsIFile> localDir;

  rv = GetProductDirectory(getter_AddRefs(localDir), aLocal);
  if (NS_FAILED(rv)) {
    return rv;
  }

  localDir.forget(aLocalFile);

  return rv;
}

//*****************************************************************************
// nsAppFileLocationProvider::nsIDirectoryServiceProvider2
//*****************************************************************************

class nsAppDirectoryEnumerator : public nsISimpleEnumerator
{
public:
  NS_DECL_ISUPPORTS

  /**
   * aKeyList is a null-terminated list of properties which are provided by aProvider
   * They do not need to be publicly defined keys.
   */
  nsAppDirectoryEnumerator(nsIDirectoryServiceProvider* aProvider,
                           const char* aKeyList[]) :
    mProvider(aProvider),
    mCurrentKey(aKeyList)
  {
  }

  NS_IMETHOD HasMoreElements(bool* aResult) override
  {
    while (!mNext && *mCurrentKey) {
      bool dontCare;
      nsCOMPtr<nsIFile> testFile;
      (void)mProvider->GetFile(*mCurrentKey++, &dontCare, getter_AddRefs(testFile));
      // Don't return a file which does not exist.
      bool exists;
      if (testFile && NS_SUCCEEDED(testFile->Exists(&exists)) && exists) {
        mNext = testFile;
      }
    }
    *aResult = mNext != nullptr;
    return NS_OK;
  }

  NS_IMETHOD GetNext(nsISupports** aResult) override
  {
    if (NS_WARN_IF(!aResult)) {
      return NS_ERROR_INVALID_ARG;
    }
    *aResult = nullptr;

    bool hasMore;
    HasMoreElements(&hasMore);
    if (!hasMore) {
      return NS_ERROR_FAILURE;
    }

    *aResult = mNext;
    NS_IF_ADDREF(*aResult);
    mNext = nullptr;

    return *aResult ? NS_OK : NS_ERROR_FAILURE;
  }

protected:
  nsCOMPtr<nsIDirectoryServiceProvider> mProvider;
  const char** mCurrentKey;
  nsCOMPtr<nsIFile> mNext;

  // Virtual destructor since subclass nsPathsDirectoryEnumerator
  // does not re-implement Release()
  virtual ~nsAppDirectoryEnumerator()
  {
  }
};

NS_IMPL_ISUPPORTS(nsAppDirectoryEnumerator, nsISimpleEnumerator)

/* nsPathsDirectoryEnumerator and PATH_SEPARATOR
 * are not used on MacOS/X. */

#if defined(XP_WIN) /* Win32 */
#define PATH_SEPARATOR ';'
#else
#define PATH_SEPARATOR ':'
#endif

class nsPathsDirectoryEnumerator final
  : public nsAppDirectoryEnumerator
{
  ~nsPathsDirectoryEnumerator() {}

public:
  /**
   * aKeyList is a null-terminated list.
   * The first element is a path list.
   * The remainder are properties provided by aProvider.
   * They do not need to be publicly defined keys.
   */
  nsPathsDirectoryEnumerator(nsIDirectoryServiceProvider* aProvider,
                             const char* aKeyList[]) :
    nsAppDirectoryEnumerator(aProvider, aKeyList + 1),
    mEndPath(aKeyList[0])
  {
  }

  NS_IMETHOD HasMoreElements(bool* aResult)
  {
    if (mEndPath)
      while (!mNext && *mEndPath) {
        const char* pathVar = mEndPath;

        // skip PATH_SEPARATORs at the begining of the mEndPath
        while (*pathVar == PATH_SEPARATOR) {
          ++pathVar;
        }

        do {
          ++mEndPath;
        } while (*mEndPath && *mEndPath != PATH_SEPARATOR);

        nsCOMPtr<nsIFile> localFile;
        NS_NewNativeLocalFile(Substring(pathVar, mEndPath),
                              true,
                              getter_AddRefs(localFile));
        if (*mEndPath == PATH_SEPARATOR) {
          ++mEndPath;
        }
        // Don't return a "file" (directory) which does not exist.
        bool exists;
        if (localFile &&
            NS_SUCCEEDED(localFile->Exists(&exists)) &&
            exists) {
          mNext = localFile;
        }
      }
    if (mNext) {
      *aResult = true;
    } else {
      nsAppDirectoryEnumerator::HasMoreElements(aResult);
    }

    return NS_OK;
  }

protected:
  const char* mEndPath;
};

NS_IMETHODIMP
nsAppFileLocationProvider::GetFiles(const char* aProp,
                                    nsISimpleEnumerator** aResult)
{
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aResult = nullptr;
  nsresult rv = NS_ERROR_FAILURE;

  if (!nsCRT::strcmp(aProp, NS_APP_PLUGINS_DIR_LIST)) {
#ifdef MOZ_WIDGET_COCOA
    // As of Java for Mac OS X 10.5 Update 10, Apple has (in effect) deprecated Java Plugin2 on
    // on OS X 10.5, and removed the soft link to it from /Library/Internet Plug-Ins/.  Java
    // Plugin2 is still present and usable, but there are no longer any links to it in the
    // "normal" locations.  So we won't be able to find it unless we look in the "non-normal"
    // location where it actually is.  Safari can use the WebKit-specific JavaPluginCocoa.bundle,
    // which (of course) is still fully supported on OS X 10.5.  But we have no alternative to
    // using Java Plugin2.  For more information see bug 668639.
    static const char* keys[] = {
      NS_APP_PLUGINS_DIR,
      NS_MACOSX_USER_PLUGIN_DIR,
      NS_MACOSX_LOCAL_PLUGIN_DIR,
      IsOSXLeopard() ? NS_MACOSX_JAVA2_PLUGIN_DIR : nullptr,
      nullptr
    };
    *aResult = new nsAppDirectoryEnumerator(this, keys);
#else
#ifdef XP_UNIX
    static const char* keys[] = { nullptr, NS_USER_PLUGINS_DIR, NS_APP_PLUGINS_DIR, NS_SYSTEM_PLUGINS_DIR, nullptr };
#else
    static const char* keys[] = { nullptr, NS_USER_PLUGINS_DIR, NS_APP_PLUGINS_DIR, nullptr };
#endif
    if (!keys[0] && !(keys[0] = PR_GetEnv("MOZ_PLUGIN_PATH"))) {
      static const char nullstr = 0;
      keys[0] = &nullstr;
    }
    *aResult = new nsPathsDirectoryEnumerator(this, keys);
#endif
    NS_ADDREF(*aResult);
    rv = NS_OK;
  }
  if (!nsCRT::strcmp(aProp, NS_APP_SEARCH_DIR_LIST)) {
    static const char* keys[] = { nullptr, NS_APP_USER_SEARCH_DIR, nullptr };
    if (!keys[0] && !(keys[0] = PR_GetEnv("MOZ_SEARCH_ENGINE_PATH"))) {
      static const char nullstr = 0;
      keys[0] = &nullstr;
    }
    *aResult = new nsPathsDirectoryEnumerator(this, keys);
    NS_ADDREF(*aResult);
    rv = NS_OK;
  }
  if (!strcmp(aProp, NS_APP_DISTRIBUTION_SEARCH_DIR_LIST)) {
    return NS_NewEmptyEnumerator(aResult);
  }
  return rv;
}

#if defined(MOZ_WIDGET_COCOA)
bool
nsAppFileLocationProvider::IsOSXLeopard()
{
  static SInt32 version = 0;

  if (!version) {
    OSErr err = ::Gestalt(gestaltSystemVersion, &version);
    if (err != noErr) {
      version = 0;
    } else {
      version &= 0xFFFF; // The system version is in the low order word
    }
  }

  return ((version >= 0x1050) && (version < 0x1060));
}
#endif
