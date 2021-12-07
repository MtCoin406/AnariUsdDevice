// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridge_h
#define UsdBridge_h

#include "UsdBridgeData.h"

struct UsdBridgeInternals;

class UsdBridge
{
  public:

    UsdBridge(const UsdBridgeSettings& settings);
    ~UsdBridge();
  
    bool OpenSession(UsdBridgeLogCallback logCallback, void* logUserData);
    bool GetSessionValid() const { return SessionValid; }
    void CloseSession();
  
    bool CreateWorld(const char* name, UsdWorldHandle& handle);
    bool CreateInstance(const char* name, UsdInstanceHandle& handle);
    bool CreateGroup(const char* name, UsdGroupHandle& handle);
    bool CreateSurface(const char* name, UsdSurfaceHandle& handle);
    bool CreateVolume(const char* name, UsdVolumeHandle& handle);
    bool CreateGeometry(const char* name, const UsdBridgeMeshData& meshData, UsdGeometryHandle& handle);
    bool CreateGeometry(const char* name, const UsdBridgeInstancerData& instancerData, UsdGeometryHandle& handle);
    bool CreateGeometry(const char* name, const UsdBridgeCurveData& curveData, UsdGeometryHandle& handle);
    bool CreateSpatialField(const char* name, UsdSpatialFieldHandle& handle);
    bool CreateMaterial(const char* name, UsdMaterialHandle& handle);
    bool CreateSampler(const char* name, UsdSamplerHandle& handle);
  
    void DeleteWorld(UsdWorldHandle handle);
    void DeleteInstance(UsdInstanceHandle handle);
    void DeleteGroup(UsdGroupHandle handle);
    void DeleteSurface(UsdSurfaceHandle handle);
    void DeleteVolume(UsdVolumeHandle handle);
    void DeleteGeometry(UsdGeometryHandle handle);
    void DeleteSpatialField(UsdSpatialFieldHandle handle);
    void DeleteMaterial(UsdMaterialHandle handle);
    void DeleteSampler(UsdSamplerHandle handle);
  
    void SetInstanceRefs(UsdWorldHandle world, const UsdInstanceHandle* instances, uint64_t numInstances, bool timeVarying, double timeStep);
    void SetGroupRef(UsdInstanceHandle instance, UsdGroupHandle group, bool timeVarying, double timeStep);
    void SetSurfaceRefs(UsdGroupHandle group, const UsdSurfaceHandle* surfaces, uint64_t numSurfaces, bool timeVarying, double timeStep);
    void SetVolumeRefs(UsdGroupHandle group, const UsdVolumeHandle* volumes, uint64_t numVolumes, bool timeVarying, double timeStep);
    void SetGeometryMaterialRef(UsdSurfaceHandle surface, UsdGeometryHandle geometry, UsdMaterialHandle material, double timeStep, double geomTimeStep, double matTimeStep);
    void SetSpatialFieldRef(UsdVolumeHandle volume, UsdSpatialFieldHandle field, double timeStep, double fieldTimeStep);
    void SetSamplerRef(UsdMaterialHandle material, UsdSamplerHandle sampler, const char* texfileName, double timeStep);
  
    void DeleteInstanceRefs(UsdWorldHandle world, bool timeVarying, double timeStep);
    void DeleteGroupRef(UsdInstanceHandle instance, bool timeVarying, double timeStep);
    void DeleteSurfaceRefs(UsdGroupHandle group, bool timeVarying, double timeStep);
    void DeleteVolumeRefs(UsdGroupHandle group, bool timeVarying, double timeStep);
    void DeleteGeometryRef(UsdSurfaceHandle surface, double timeStep);
    void DeleteSpatialFieldRef(UsdVolumeHandle volume, double timeStep);
    void DeleteMaterialRef(UsdSurfaceHandle surface, double timeStep);
    void DeleteSamplerRef(UsdMaterialHandle material, double timeStep);
  
    void UpdateBeginEndTime(double timeStep);
    void SetInstanceTransform(UsdInstanceHandle instance, float* transform, bool timeVarying, double timeStep);
    void SetGeometryData(UsdGeometryHandle geometry, const UsdBridgeMeshData& meshData, double timeStep);
    void SetGeometryData(UsdGeometryHandle geometry, const UsdBridgeInstancerData& instancerData, double timeStep);
    void SetGeometryData(UsdGeometryHandle geometry, const UsdBridgeCurveData& curveData, double timeStep);
    void SetVolumeData(UsdSpatialFieldHandle field, const UsdBridgeVolumeData& volumeData, double timeStep);
    void SetMaterialData(UsdMaterialHandle material, const UsdBridgeMaterialData& matData, double timeStep);
    void SetSamplerData(UsdSamplerHandle sampler, const UsdBridgeSamplerData& samplerData, double timeStep);
  
    void SaveScene();

    void GarbageCollect();

    //
    // Static parameter interface
    //

    static void SetConnectionLogVerbosity(int logVerbosity); // 0 <= logVerbosity <= 4, 0 is quietest

  protected: 

    template<typename GeomDataType>
    bool CreateGeometryTemplate(const char* name, const GeomDataType& geomData, UsdGeometryHandle& handle);

    template<typename GeomDataType>
    void SetGeometryDataTemplate(UsdGeometryHandle geometry, const GeomDataType& geomData, double timeStep);

    UsdBridgeInternals* Internals;
  
    bool SessionValid;
};

#endif