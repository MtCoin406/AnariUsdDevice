// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdParameterizedObject.h"

class UsdBridge;
class UsdDevice;

template<class T, class D, class H>
class UsdBridgedBaseObject : public UsdBaseObject, public UsdParameterizedObject<T, D>
{
  public:
    typedef UsdParameterizedObject<T, D> ParamClass;

    UsdBridgedBaseObject(ANARIDataType t, const char* name, UsdBridge* bridge)
      : UsdBaseObject(t)
      , uniqueName(name)
      , usdBridge(bridge)
    {
    }

    H getUsdHandle() const { return usdHandle; }

    const char* getName() const { return ParamClass::paramData.usdName ? ParamClass::paramData.usdName : uniqueName; }

    void formatUsdName()
    {
      char* name = const_cast<char*>(ParamClass::paramData.usdName);
      assert(strlen(name) > 0);

      auto letter = [](unsigned c) { return ((c - 'A') < 26) || ((c - 'a') < 26); };
      auto number = [](unsigned c) { return (c - '0') < 10; };
      auto under = [](unsigned c) { return c == '_'; };

      unsigned x = *name;
      if (!letter(x) && !under(x)) { *name = '_'; }
      x = *(++name);
      while (x != '\0') 
      {
        if(!letter(x) && !number(x) && !under(x))
          *name = '_';
        x = *(++name);
      };
    }

    bool filterNameParam(const char *name,
      ANARIDataType type,
      const void *mem,
      UsdDevice* device)
    {
      const char* objectName = static_cast<const char*>(mem);

      if (type == ANARI_STRING)
      {
        if ((strcmp(name, "name") == 0))
        {
          if (strcmp(objectName, "") == 0)
          {
            reportStatusThroughDevice(device,
              this, ANARI_OBJECT, ANARI_SEVERITY_WARNING, ANARI_STATUS_NO_ERROR,
              "%s: ANARI object %s cannot be an empty string, using auto-generated name instead.", getName(), "name");
          }
          else
          {
            ParamClass::setParam(name, type, mem, device);
            ParamClass::setParam("usd::name", type, mem, device);
            formatUsdName();
          }
          return false;
        }
        else if (strcmp(name, "usd::name") == 0)
        {
          reportStatusThroughDevice(device,
            this, ANARI_OBJECT, ANARI_SEVERITY_WARNING, ANARI_STATUS_NO_ERROR,
            "%s parameter '%s' cannot be set, only read with getProperty().", getName(), "usd::name");
          return false;
        }
      } 
      return true;
    }

    int getProperty(const char *name,
      ANARIDataType type,
      void *mem,
      uint64_t size,
      UsdDevice* device)
    {
      if (type == ANARI_STRING && strcmp(name, "usd::name") == 0)
      {
        snprintf((char*)mem, size, "%s", ParamClass::paramData.usdName);
        return 1;
      }
      else if (type == ANARI_UINT64 && strcmp(name, "usd::name.size") == 0)
      {
        if (checkSizeOnStringLengthProperty(device, this, size, "usd::name.size"))
        {
          uint64_t nameLen = strlen(ParamClass::paramData.usdName)+1;
          memcpy(mem, &nameLen, size);
        }
        return 1;
      }
      else if (type == ANARI_STRING && strcmp(name, "usd::primpath") == 0)
      {
        const char* primPath = usdBridge->GetPrimPath(&usdHandle);
        snprintf((char*)mem, size, "%s", primPath);
        return 1;
      }
      else if (type == ANARI_UINT64 && strcmp(name, "usd::primpath.size") == 0)
      {
        if (checkSizeOnStringLengthProperty(device, this, size, "usd::primpath.size"))
        {
          const char* primPath = usdBridge->GetPrimPath(&usdHandle);
          uint64_t nameLen = strlen(primPath)+1;
          memcpy(mem, &nameLen, size);
        }
        return 1;
      }
      return 0;
    }

  protected:
    typedef UsdBridgedBaseObject<T,D,H> BridgedBaseObjectType;

    const char* uniqueName;
    UsdBridge* usdBridge;
    H usdHandle;
};