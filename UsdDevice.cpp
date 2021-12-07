﻿// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdDevice.h"
#include "UsdBridge/UsdBridge.h"
#include "UsdBaseObject.h"
#include "UsdDataArray.h"
#include "UsdGeometry.h"
#include "UsdSpatialField.h"
#include "UsdSurface.h"
#include "UsdVolume.h"
#include "UsdInstance.h"
#include "UsdGroup.h"
#include "UsdMaterial.h"
#include "UsdSampler.h"
#include "UsdTransferFunction.h"
#include "UsdWorld.h"
#include "UsdRenderer.h"
#include "UsdFrame.h"

#include <cstdarg>
#include <cstdio>
#include <set>
#include <memory>

static char deviceName[] = "usd";

extern "C"
{
#ifdef WIN32
  USDDevice_INTERFACE void __cdecl anari_library_usd_init()
#else
  void anari_library_usd_init()
#endif
  {
    printf("...loaded usd library!\n");
    anari::Device::registerType<UsdDevice>(deviceName);
  }
}

ANARI_DEFINE_LIBRARY_GET_DEVICE_SUBTYPES(reference, libdata)
{
  static const char *devices[] = { deviceName, nullptr };
  return devices;
}

template <typename T>
inline void writeToVoidP(void *_p, T v)
{
  T *p = (T *)_p;
  *p = v;
}

class UsdDeviceSettings
{
public:
  std::string HostName;
  std::string OutputPath;
  bool CreateNewSession;
  bool BinaryOutput;
};

class UsdDeviceInternals
{
public:
  UsdDeviceInternals()
  {
  }

  bool CreateNewBridge(UsdBridgeLogCallback bridgeStatusFunc, void* userData)
  {
    if (bridge.get())
      bridge->CloseSession();

    UsdBridgeSettings bridgeSettings = {
      settings.HostName.c_str(),
      settings.OutputPath.c_str(),
      settings.CreateNewSession,
      settings.BinaryOutput
    };

    bridge = std::make_unique<UsdBridge>(bridgeSettings);

    bridgeStatusFunc(UsdBridgeLogLevel::STATUS, userData, "Initializing UsdBridge Session");

    bool createSuccess = bridge->OpenSession(bridgeStatusFunc, userData);

    if (!createSuccess)
    {
      bridge = nullptr;
      bridgeStatusFunc(UsdBridgeLogLevel::STATUS, userData, "UsdBridge Session initialization failed.");
    }
    else
    {
      bridgeStatusFunc(UsdBridgeLogLevel::STATUS, userData, "UsdBridge Session initialization successful.");
    }

    return createSuccess;
  }

  UsdDeviceSettings settings; // Settings lifetime should encapsulate bridge lifetime
  std::unique_ptr<UsdBridge> bridge;

  std::set<std::string> uniqueNames;
};


DEFINE_PARAMETER_MAP(UsdDevice,
  REGISTER_PARAMETER_MACRO("usd::serialize.hostname", ANARI_STRING, hostName)
  REGISTER_PARAMETER_MACRO("usd::serialize.outputpath", ANARI_STRING, outputPath)
  REGISTER_PARAMETER_MACRO("usd::serialize.newsession", ANARI_BOOL, createNewSession)
  REGISTER_PARAMETER_MACRO("usd::serialize.outputbinary", ANARI_BOOL, outputBinary)
  REGISTER_PARAMETER_MACRO("usd::timestep", ANARI_FLOAT64, timeStep)
)

UsdDevice::UsdDevice()
  : internals(std::make_unique<UsdDeviceInternals>())
{}

UsdDevice::~UsdDevice()
{
  //internals->bridge->SaveScene(); //Uncomment to test cleanup of usd files.

#ifdef CHECK_MEMLEAKS
  assert(allocatedObjects.empty());
#endif
}

void UsdDevice::reportStatus(void* source,
  ANARIDataType sourceType,
  ANARIStatusSeverity severity,
  ANARIStatusCode statusCode,
  const char *format, ...)
{
  va_list arglist;

  va_start(arglist, format);
  reportStatus(source, sourceType, severity, statusCode, format, arglist);
  va_end(arglist);
}

static void reportBridgeStatus(UsdBridgeLogLevel level, void* device, const char *message)
{
  ANARIStatusSeverity severity = UsdBridgeLogLevelToAnariSeverity(level);

  va_list arglist;
  ((UsdDevice*)device)->reportStatus(nullptr, ANARI_UNKNOWN, severity, ANARI_STATUS_NO_ERROR, message, arglist);
}

void UsdDevice::reportStatus(void* source,
  ANARIDataType sourceType,
  ANARIStatusSeverity severity,
  ANARIStatusCode statusCode,
  const char *format,
  va_list& arglist)
{
  va_list arglist_copy;
  va_copy(arglist_copy, arglist);
  int count = std::vsnprintf(nullptr, 0, format, arglist);

  lastStatusMessage.resize(count + 1);

  std::vsnprintf(lastStatusMessage.data(), count + 1, format, arglist_copy);
  va_end(arglist_copy);

  if (statusFunc != nullptr)
  {
    statusFunc(
      statusUserData,
      (ANARIDevice)this,
      (ANARIObject)source,
      sourceType,
      severity,
      statusCode,
      lastStatusMessage.data());
  }
}

void UsdDevice::deviceSetParameter(
  const char *id, ANARIDataType type, const void *mem)
{
  if (std::strcmp(id, "usd::garbagecollect") == 0)
  {
    // Perform garbage collection on usd objects (needs to move into the user interface)
    if(internals->bridge)
      internals->bridge->GarbageCollect();
  }
  else if(std::strcmp(id, "usd::removeunusednames") == 0)
  {
    internals->uniqueNames.clear();
  }
  else if (std::strcmp(id, "usd::connection.logverbosity") == 0) // 0 <= verbosity <= 4, with 4 being the loudest
  {
    if(type == ANARI_INT32)
      UsdBridge::SetConnectionLogVerbosity(*((int*)mem));
  }
  else if (std::strcmp(id, "statusCallback") == 0 && type == ANARI_STATUS_CALLBACK)
  {
    statusFunc = (ANARIStatusCallback)mem;
  }
  else if (std::strcmp(id, "statusCallbackUserData") == 0 && type == ANARI_VOID_POINTER)
  {
    statusUserData = (void *)mem;
  }
  else
  {
    setParam(id, type, mem, this);

    if (std::strcmp(id, "usd::timestep") == 0)
    {
      if(internals->bridge)
        internals->bridge->UpdateBeginEndTime(paramData.timeStep);
    }
  }
}

void UsdDevice::deviceUnsetParameter(const char * id)
{
  if (std::strcmp(id, "statusCallback"))
  {
    statusFunc = nullptr;
  }
  else if (std::strcmp(id, "statusCallbackUserData"))
  {
    statusUserData = nullptr;
  }
  else if (std::strcmp(id, "usd::garbagecollect") != 0
    && std::strcmp(id, "usd::removeunusednames") != 0)
  {
    resetParam(id);
  }
}

int UsdDevice::deviceImplements(const char *)
{
  return 0;
}

void UsdDevice::deviceCommit()
{
  statusFunc = userSetStatusFunc ? userSetStatusFunc : defaultStatusCallback();
  statusUserData = userSetStatusUserData ? userSetStatusUserData : defaultStatusCallbackUserPtr();

  if (!paramData.outputPath)
  {
    reportStatus(this, ANARI_DEVICE, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "Usd Device parameter 'serialize.outputpath' not set.");
    return;
  }
  if (paramData.hostName)
  {
    internals->settings.HostName = paramData.hostName;
  }
  internals->settings.OutputPath = paramData.outputPath;
  internals->settings.CreateNewSession = paramData.createNewSession;
  internals->settings.BinaryOutput = paramData.outputBinary;

  if (!internals->CreateNewBridge(&reportBridgeStatus, this))
  {
    reportStatus(this, ANARI_DEVICE, ANARI_SEVERITY_ERROR, ANARI_STATUS_UNKNOWN_ERROR, "Usd Bridge failed to load");
  }
}

void UsdDevice::deviceRetain()
{
  this->refInc();
}

void UsdDevice::deviceRelease()
{
  this->refDec();
}

ANARIArray UsdDevice::CreateDataArray(void *appMemory,
  ANARIMemoryDeleter deleter,
  void *userData,
  ANARIDataType dataType,
  uint64_t numItems1,
  int64_t byteStride1,
  uint64_t numItems2,
  int64_t byteStride2,
  uint64_t numItems3,
  int64_t byteStride3)
{
  if (!appMemory)
  {
    UsdDataArray* object = new UsdDataArray(dataType, numItems1, numItems2, numItems3, this);
#ifdef CHECK_MEMLEAKS
    LogAllocation(object);
#endif

    return (ANARIArray)(object);
  }
  else
  {
    UsdDataArray* object = new UsdDataArray(appMemory, deleter, userData,
      dataType, numItems1, byteStride1, numItems2, byteStride2, numItems3, byteStride3,
      this);
#ifdef CHECK_MEMLEAKS
    LogAllocation(object);
#endif

    return (ANARIArray)(object);
  }
}

ANARIArray1D UsdDevice::newArray1D(void *appMemory,
  ANARIMemoryDeleter deleter,
  void *userData,
  ANARIDataType type,
  uint64_t numItems,
  uint64_t byteStride)
{
  return (ANARIArray1D)CreateDataArray(appMemory, deleter, userData,
    type, numItems, byteStride, 1, 0, 1, 0);
}

ANARIArray2D UsdDevice::newArray2D(void *appMemory,
  ANARIMemoryDeleter deleter,
  void *userData,
  ANARIDataType type,
  uint64_t numItems1,
  uint64_t numItems2,
  uint64_t byteStride1,
  uint64_t byteStride2)
{
  return (ANARIArray2D)CreateDataArray(appMemory, deleter, userData,
    type, numItems1, byteStride1, numItems2, byteStride2, 1, 0);
}

ANARIArray3D UsdDevice::newArray3D(void *appMemory,
  ANARIMemoryDeleter deleter,
  void *userData,
  ANARIDataType type,
  uint64_t numItems1,
  uint64_t numItems2,
  uint64_t numItems3,
  uint64_t byteStride1,
  uint64_t byteStride2,
  uint64_t byteStride3)
{
  return (ANARIArray3D)CreateDataArray(appMemory, deleter, userData,
    type, numItems1, byteStride1, numItems2, byteStride2, numItems3, byteStride3);
}

void * UsdDevice::mapArray(ANARIArray array)
{
  return ((UsdDataArray*)array)->map(this);
}

void UsdDevice::unmapArray(ANARIArray array)
{
  ((UsdDataArray*)array)->unmap(this);
}

void UsdDevice::arrayRangeUpdated1D(ANARIArray1D, uint64_t startIndex1, uint64_t elementCount1)
{
  reportStatus(this, ANARI_DEVICE, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_OPERATION, "Usd Device arrayRangeUpdated1D not implemented yet.");
}

void UsdDevice::arrayRangeUpdated2D(ANARIArray2D, uint64_t startIndex1, uint64_t startIndex2, uint64_t elementCount1, uint64_t elementCount2)
{
  reportStatus(this, ANARI_DEVICE, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_OPERATION, "Usd Device arrayRangeUpdated2D not implemented yet.");
}

void UsdDevice::arrayRangeUpdated3D(ANARIArray3D, uint64_t startIndex1, uint64_t startIndex2, uint64_t startIndex3, uint64_t elementCount1, uint64_t elementCount2, uint64_t elementCount3)
{
  reportStatus(this, ANARI_DEVICE, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_OPERATION, "Usd Device arrayRangeUpdated3D not implemented yet.");
}

ANARISampler UsdDevice::newSampler(const char *type)
{
  const char* name = makeUniqueName("Sampler");
  UsdSampler* object = new UsdSampler(name, internals->bridge.get());
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARISampler)(object);
}

ANARIMaterial UsdDevice::newMaterial(const char *material_type)
{
  const char* name = makeUniqueName("Material");
  UsdMaterial* object = new UsdMaterial(name, material_type, internals->bridge.get(), this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIMaterial)(object);
}

ANARIGeometry UsdDevice::newGeometry(const char *type)
{
  const char* name = makeUniqueName("Geometry");
  UsdGeometry* object = new UsdGeometry(name, type, internals->bridge.get(), this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIGeometry)(object);
}

ANARISpatialField UsdDevice::newSpatialField(const char * type)
{
  const char* name = makeUniqueName("SpatialField");
  UsdSpatialField* object = new UsdSpatialField(name, type, internals->bridge.get());
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARISpatialField)(object);
}

ANARISurface UsdDevice::newSurface()
{
  const char* name = makeUniqueName("Surface");
  UsdSurface* object = new UsdSurface(name, internals->bridge.get(), this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARISurface)(object);
}

ANARIVolume UsdDevice::newVolume(const char *type)
{
  const char* name = makeUniqueName("Volume");
  UsdVolume* object = new UsdVolume(name, internals->bridge.get(), this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIVolume)(object);
}

ANARIGroup UsdDevice::newGroup()
{
  const char* name = makeUniqueName("Group");
  UsdGroup* object = new UsdGroup(name, internals->bridge.get(), this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIGroup)(object);
}

ANARIInstance UsdDevice::newInstance()
{
  const char* name = makeUniqueName("Instance");
  UsdInstance* object = new UsdInstance(name, internals->bridge.get(), this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIInstance)(object);
}

ANARIWorld UsdDevice::newWorld()
{
  const char* name = makeUniqueName("World");
  UsdWorld* object = new UsdWorld(name, internals->bridge.get());
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIWorld)(object);
}

ANARIRenderer UsdDevice::newRenderer(const char *type)
{
  UsdRenderer* object = new UsdRenderer(internals->bridge.get());
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIRenderer)(object);
}

void UsdDevice::renderFrame(ANARIFrame frame)
{
  UsdRenderer* ren = ((UsdFrame*)frame)->getRenderer();
  if(ren)
    ren->saveUsd();
}

const char* UsdDevice::makeUniqueName(const char* name)
{
  std::string proposedBaseName(name);
  proposedBaseName.append("_");

  int postFix = 0;
  std::string proposedName = proposedBaseName + std::to_string(postFix);

  auto empRes = internals->uniqueNames.emplace(proposedName);
  while (!empRes.second)
  {
    ++postFix;
    proposedName = proposedBaseName + std::to_string(postFix);
    empRes = internals->uniqueNames.emplace(proposedName);
  }

  return empRes.first->c_str();
}

bool UsdDevice::nameExists(const char* name)
{
  return internals->uniqueNames.find(name) != internals->uniqueNames.end();
}

int UsdDevice::getProperty(ANARIObject object,
    const char *name,
    ANARIDataType type,
    void *mem,
    uint64_t size,
    uint32_t mask)
{
  if ((void *)object == (void *)this)
  {
    std::string_view prop = name;
    if (prop == "version" && type == ANARI_INT32) {
      writeToVoidP(mem, DEVICE_VERSION);
      return 1;
    }
  }
  else
    return ((UsdBaseObject*)object)->getProperty(name, type, mem, size, this);

  return 0;
}

ANARIFrame UsdDevice::frameCreate()
{
  UsdFrame* object = new UsdFrame(internals->bridge.get());
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIFrame)(object);
}

const void * UsdDevice::frameBufferMap(
  ANARIFrame fb, const char *channel)
{
  if (fb)
    return ((UsdFrame*)fb)->mapBuffer(channel);
  return nullptr;
}

void UsdDevice::frameBufferUnmap(ANARIFrame fb, const char *channel)
{
  if (fb)
    return ((UsdFrame*)fb)->unmapBuffer(channel);
}

void UsdDevice::setParameter(ANARIObject object,
  const char *name,
  ANARIDataType type,
  const void *mem)
{
  if (object)
    ((UsdBaseObject*)object)->filterSetParam(name, type, mem, this);
}

void UsdDevice::unsetParameter(ANARIObject object, const char * name)
{
  if (object)
    ((UsdBaseObject*)object)->filterResetParam(name);
}

void UsdDevice::release(ANARIObject object)
{
  UsdBaseObject* baseObject = (UsdBaseObject*)object;

  if (baseObject)
  {
    bool privatizeArray = baseObject->getType() == ANARI_ARRAY
      && baseObject->useCount(anari::RefType::INTERNAL) > 0
      && baseObject->useCount(anari::RefType::PUBLIC) == 1;

#ifdef CHECK_MEMLEAKS
    LogDeallocation(baseObject);
#endif
    if (baseObject)
      baseObject->refDec(anari::RefType::PUBLIC);

    if (privatizeArray)
      ((UsdDataArray*)baseObject)->privatize();
  }
}

void UsdDevice::retain(ANARIObject object)
{
  if (object)
    ((UsdBaseObject*)object)->refInc(anari::RefType::PUBLIC);
}

void UsdDevice::commit(ANARIObject object)
{
  if(object)
    ((UsdBaseObject*)object)->commit(this);
}

#ifdef CHECK_MEMLEAKS
void UsdDevice::LogAllocation(const anari::RefCounted* ptr)
{
  allocatedObjects.push_back(ptr);
}

void UsdDevice::LogDeallocation(const anari::RefCounted* ptr)
{
  if (ptr && ptr->useCount() == 1)
  {
    auto it = std::find(allocatedObjects.begin(), allocatedObjects.end(), ptr);
    assert(it != allocatedObjects.end());
    allocatedObjects.erase(it);
  }
}
#endif

