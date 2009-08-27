/* vim: le=unix sw=2 : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Songbird Distribution Stub Helper.
 *
 * The Initial Developer of the Original Code is
 * POTI <http://www.songbirdnest.com/>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mook <mook@songbirdnest.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "error.h"
#include "stringconvert.h"
#include "debug.h"
#include "commands.h"
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdlib.h>

#ifndef UNICODE
#error This only supports the UNICODE configuration (no SBCS/MBCS)
#endif

#define MAX_LONG_PATH 0x8000 /* 32767 + 1, maximum size for \\?\ style paths */

#define NS_ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))

tstring ResolvePathName(std::string aSrc) {
  std::wstring src(ConvertUTF8ToUTF16(aSrc));
  std::wstring::iterator begin(src.begin());
  #if DEBUG
    DebugMessage("Resolving path name %S", src.c_str());
  #endif
  // replace all forward slashes with backward ones
  std::wstring::size_type i = src.find(L'/');
  while (i != std::wstring::npos) {
    src[i] = L'\\';
    i = src.find(L'/', i);
  }
  if (begin != src.end()) {
    if (L'$' == *begin) {
      std::wstring::iterator next = begin;
      ++++next; // skip two characters
      src.replace(begin, next, GetAppDirectory());
    }
    WCHAR buffer[MAX_LONG_PATH + 1];
    DWORD length = SearchPath(GetDistIniDirectory().c_str(),
                              src.c_str(),
                              NULL,
                              MAX_LONG_PATH,
                              buffer,
                              NULL);
    if (length > 0) {
      buffer[length] = '\0';
      src.assign(buffer, length + 1);
    } else {
      DebugMessage("Failed to resolve path name %S", src.c_str());
    }
  }
  #if DEBUG
    DebugMessage("Resolved path name %S", src.c_str());
  #endif
  return src;
}

static int DoFileCommand(UINT aFunction, const char* aDescription, std::string aSrc, std::string aDest, bool aRecursive) {
  std::wstring src(ResolvePathName(aSrc)), dest(ResolvePathName(aDest));
  if (!dest.empty()) {
    switch (*--dest.end()) {
      case L'\\':
      case L'/':
        int result = SHCreateDirectory(NULL, dest.c_str());
        switch (result) {
          case ERROR_BAD_PATHNAME:
            DebugMessage("Failed to create directory %S: the path is invalid", dest.c_str());
            return DH_ERROR_WRITE;
          case ERROR_FILENAME_EXCED_RANGE:
            DebugMessage("Failed to create directory %S: the path is too long", dest.c_str());
            return DH_ERROR_WRITE;
          case ERROR_FILE_EXISTS:
          case ERROR_ALREADY_EXISTS:
            // that's fine, since we never checked that it didn't exist
            break;
          case ERROR_CANCELLED:
            DebugMessage("Failed to create directory %S: the operation was cancelled", dest.c_str());
            return DH_ERROR_WRITE;
          case ERROR_SUCCESS:
            break;
          default:
            DebugMessage("Failed to create directory %S: unknown reason %ul", dest.c_str(), ::GetLastError());
            return DH_ERROR_WRITE;
        }
    }
  }
  
  // if delete and not recursive, make sure the directory is empty
  if (FO_DELETE == aFunction && !aRecursive) {
    DWORD fileAttr = GetFileAttributes(src.c_str());
    if ((fileAttr != INVALID_FILE_ATTRIBUTES) && (fileAttr & FILE_ATTRIBUTE_DIRECTORY)) {
      std::wstring findPath(src);
      findPath.append(L"\\*");
      WIN32_FIND_DATA findData;
      DebugMessage("Looking in %S", findPath.c_str());
      HANDLE hFind = ::FindFirstFile(findPath.c_str(), &findData);
      if (hFind != INVALID_HANDLE_VALUE) {
        DebugMessage("Cannot delete %S non-recursively, it conatins files", src.c_str());
        return DH_ERROR_OK;
      }
    }
  }
  
  src.append(L"\0\0", 2);
  dest.append(L"\0\0", 2);
  SHFILEOPSTRUCT ops;
  ops.hwnd = NULL; // Desktop?
  ops.wFunc = aFunction;
  ops.pFrom = src.c_str();
  ops.pTo = dest.c_str();
  ops.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_NOERRORUI | FOF_SILENT | FOF_NO_UI;
  // XXX Mook: force no recursion for now; preed thinks it's likely to be dangerous
  // and we ended up not having an immediate use case for it
  if (true || !aRecursive) {
    ops.fFlags |= FOF_NORECURSION;
  }
  #if DEBUG
    DebugMessage("%sing %S to %S (%srecursive)",
                 aDescription,
                 src.c_str(),
                 dest.c_str(),
                 aRecursive ? "" : "not ");
  #endif
  if (::SHFileOperation(&ops)) {
    LogMessage("Failed to %s %S to %S", aDescription, src.c_str(), dest.c_str());
    return DH_ERROR_UNKNOWN;
  }
  return DH_ERROR_OK;
}


int CommandCopyFile(std::string aSrc, std::string aDest, bool aRecursive) {
  return DoFileCommand(FO_COPY, "copy", aSrc, aDest, aRecursive);
}

int CommandMoveFile(std::string aSrc, std::string aDest, bool aRecursive) {
  return DoFileCommand(FO_MOVE, "move", aSrc, aDest, aRecursive);
}

int CommandDeleteFile(std::string aFile, bool aRecursive) {
  return DoFileCommand(FO_DELETE, "delete", aFile, std::string(""), aRecursive);
}

int CommandExecuteFile(std::string aExecutable, const std::vector<std::string>& aArgs) {
  tstring arg(_T(" \""));
  std::vector<std::string>::const_iterator it, end = aArgs.end();
  for (it = aArgs.begin(); it < end; ++it) {
    DebugMessage("<%s>", it->c_str());
    arg.append(FilterSubstitution(ConvertUTF8toUTFn(*it)));
    arg.append(_T("\" \""));
  }
  arg.erase(arg.length() - 2);  // remove the excess quote at the end
                                // if no args, comeletely earses the string
  
  DebugMessage("<%s> <%s>", aExecutable.c_str(), arg.c_str());
  HINSTANCE hInst = ::ShellExecuteW(NULL,
                                    L"open",
                                    ConvertUTF8ToUTF16(aExecutable).c_str(),
                                    arg.c_str(),
                                    NULL,
                                    SW_SHOWDEFAULT);
  return ((ULONG_PTR)hInst > 32 ? DH_ERROR_OK : DH_ERROR_UNKNOWN);
}

tstring FilterSubstitution(tstring aString) {
  tstring result = aString;
  tstring::size_type start = 0, end = tstring::npos;
  while (true) {
    start = result.find(tstring::value_type('$'), start);
    if (start == tstring::npos) {
      break;
    }
    end = result.find(tstring::value_type('$'), start + 1);
    if (end == tstring::npos) {
      break;
    }
    tstring variable = result.substr(start + 1, end - 1);
    if (variable == _T("APPDIR")) {
      tstring appdir = GetAppDirectory();
      DebugMessage("AppDir: %s", appdir.c_str());
      result.replace(start, end-start+1, appdir);
      start += appdir.length();
      continue;
    }
    start = end + 1;
  }
  return result;
}

std::vector<std::string> ParseCommandLine(const std::string& aCommandLine) {
  static const char WHITESPACE[] = " \t\r\n";
  std::vector<std::string> args;
  std::string::size_type prev = 0, offset;
  offset = aCommandLine.find_last_not_of(WHITESPACE);
  if (offset == std::string::npos) {
    // there's nothing that's not whitespace, don't bother
    return args;
  }
  std::string commandLine = aCommandLine.substr(0, offset + 1);
  std::string::size_type length = commandLine.length();
  do {
    prev = commandLine.find_first_not_of(WHITESPACE, prev);
    if (prev == std::string::npos) {
      // nothing left that's not whitespace
      break;
    }
    if (commandLine[prev] == '"') {
      // start of quoted param
      ++prev; // eat the quote
      offset = commandLine.find('"', prev);
      if (offset == std::string::npos) {
        // no matching end quote; assume it lasts to the end of the command
        offset = commandLine.length();
      }
    } else {
      // unquoted
      offset = commandLine.find_first_of(WHITESPACE, prev);
      if (offset == std::string::npos) {
        offset = commandLine.length();
      }
    }
    args.push_back(commandLine.substr(prev, offset - prev));
    prev = offset + 1;
  } while (prev < length);
  
  return args;
}

tstring GetAppDirectory() {
  WCHAR buffer[MAX_LONG_PATH + 1] = {0}; 
  HMODULE hExeModule = ::GetModuleHandle(NULL);
  DWORD length = ::GetModuleFileName(hExeModule, buffer, MAX_LONG_PATH);
  buffer[MAX_LONG_PATH - 1] = '\0';
  if (length != MAX_LONG_PATH) {
    buffer[length] = '\0';
  }
  tstring result(buffer);
  tstring::size_type sep = result.rfind(L'\\');
  if (sep != tstring::npos) {
    result.erase(sep + 1);
  }
  #if DEBUG
    DebugMessage("Found app directory %S", result.c_str());
  #endif
  return result;
}

tstring gDistIniDirectory;
tstring GetDistIniDirectory(const TCHAR *aPath) {
  if (aPath) {
    TCHAR buffer[MAX_PATH];
    _tcsncpy(buffer, GetAppDirectory().c_str(), NS_ARRAY_LENGTH(buffer));
    buffer[NS_ARRAY_LENGTH(buffer) - 1] = _T('\0');
    // the PathAppend call will correctly copy aPath over if it is already an
    // absolute path; otherwise, it will append aPath to the app directory
    if (!::PathAppend(buffer, aPath)) {
      DebugMessage("Failed to resolve dist.ini path %S", aPath);
      return tstring(_T(""));
    }

    // if the given file doesn't exist, bail (because there are no actions)
    if (!::PathFileExists(buffer)) {
      DebugMessage("File %S doesn't exist, bailing", buffer);
      return tstring(_T(""));
    }

    // now remove the file name
    if (!::PathRemoveFileSpec(buffer)) {
      DebugMessage("Failed to find directory name for %S", buffer);
      return tstring(_T(""));
    }
    if (!::PathAddBackslash(buffer)) {
      DebugMessage("Failed to add trailing backslash to %S", buffer);
      return tstring(_T(""));
    }

    #if DEBUG
      DebugMessage("found distribution path %S", buffer);
    #endif
    gDistIniDirectory = buffer;
  }
  return gDistIniDirectory;
}

std::string GetLeafName(std::string aSrc) {
  std::string::size_type slash, backslash;
  backslash = aSrc.rfind('\\');
  if (backslash != std::string::npos) {
    return aSrc.substr(backslash);
  }
  slash = aSrc.rfind('/');
  if (slash != std::string::npos) {
    return aSrc.substr(slash);
  }
  return aSrc;
}

void ShowFatalError(const char* fmt, ...) {
  tstring appIni = ResolvePathName("$/application.ini");
  tstring bakIni = ResolvePathName("$/broken.application.ini");
  _tunlink(bakIni.c_str());
  _trename(appIni.c_str(), bakIni.c_str());

  if (_tgetenv(_T("DISTHELPER_SILENT_FAILURE"))) {
    return;
  }

  va_list args;
  int len;
  TCHAR *buffer;

  // retrieve the variable arguments
  va_start(args, fmt);
  tstring msg(_T("An application update error has occurred; please re-install ")
              _T("the application.  Your media has not been affected.\n\n")
              _T("Related deatails:\n\n"));
  msg.append(ConvertUTF8ToUTF16(fmt));
  
  len = _vsctprintf(msg.c_str(), args) // _vscprintf doesn't count
          + 1;                         // terminating '\0'
  
  buffer = (TCHAR*)malloc(len * sizeof(TCHAR));

  _vstprintf(buffer, msg.c_str(), args);
  ::MessageBox(NULL,
               buffer,
               _T("Update Distribution Helper"),
               MB_OK | MB_ICONERROR);

  free(buffer);
  va_end(args);
}
