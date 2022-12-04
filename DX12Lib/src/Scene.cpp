#include "DX12LibPCH.h"

#include <dx12lib/Scene.h>

#include <dx12lib/CommandList.h>
#include <dx12lib/Device.h>
#include <dx12lib/Material.h>
#include <dx12lib/Mesh.h>
#include <dx12lib/SceneNode.h>
#include <dx12lib/Texture.h>
#include <dx12lib/VertexTypes.h>
#include <dx12lib/Visitor.h>

using namespace dx12lib;

// A progress handler for Assimp
class ProgressHandler : public Assimp::ProgressHandler
{
public:
    ProgressHandler( const Scene& scene, const std::function<bool( float )> progressCallback )
    : m_Scene( scene )
    , m_ProgressCallback( progressCallback )
    {}

    virtual bool Update( float percentage ) override
    {
        // Invoke the progress callback
        if ( m_ProgressCallback )
        {
            return m_ProgressCallback( percentage );
        }

        return true;
    }

private:
    const Scene&                 m_Scene;
    std::function<bool( float )> m_ProgressCallback;
};

// Helper function to create an DirectX::BoundingBox from an aiAABB.
inline DirectX::BoundingBox CreateBoundingBox( const aiAABB& aabb )
{
    XMVECTOR min = XMVectorSet( aabb.mMin.x, aabb.mMin.y, aabb.mMin.z, 1.0f );
    XMVECTOR max = XMVectorSet( aabb.mMax.x, aabb.mMax.y, aabb.mMax.z, 1.0f );

    DirectX::BoundingBox bb;
    BoundingBox::CreateFromPoints( bb, min, max );

    return bb;
}

bool Scene::LoadSceneFromFile( CommandList& commandList, const std::wstring& fileName,
                               const std::function<bool( float )>& loadingProgress )
{

    fs::path filePath   = fileName;
    fs::path exportPath = fs::path( filePath ).replace_extension( "assbin" );

    fs::path parentPath;
    if ( filePath.has_parent_path() )
    {
        parentPath = filePath.parent_path();
    }
    else
    {
        parentPath = fs::current_path();
    }

    Assimp::Importer importer;
    const aiScene*   scene;

    importer.SetProgressHandler( new ProgressHandler( *this, loadingProgress ) );

    // Check if a preprocessed file exists.
    if ( fs::exists( exportPath ) && fs::is_regular_file( exportPath ) )
    {
        scene = importer.ReadFile( exportPath.string(), aiProcess_GenBoundingBoxes );
    }
    else
    {
        // File has not been preprocessed yet. Import and processes the file.
        importer.SetPropertyFloat( AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80.0f );
        importer.SetPropertyInteger( AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE );

        unsigned int preprocessFlags = aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_OptimizeGraph |
                                       aiProcess_ConvertToLeftHanded | aiProcess_GenBoundingBoxes;
        scene = importer.ReadFile( filePath.string(), preprocessFlags );

        if ( scene )
        {
            // Export the preprocessed scene file for faster loading next time.
            Assimp::Exporter exporter;
            exporter.Export( scene, "assbin", exportPath.string(), 0 );
        }
    }

    if ( !scene )
    {
        return false;
    }

    ImportScene( commandList, *scene, parentPath );

    return true;
}

bool Scene::LoadSceneFromString( CommandList& commandList, const std::string& sceneStr, const std::string& format )
{
    Assimp::Importer importer;
    const aiScene*   scene = nullptr;

    importer.SetPropertyFloat( AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80.0f );
    importer.SetPropertyInteger( AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE );

    unsigned int preprocessFlags =
        aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded | aiProcess_GenBoundingBoxes;

    scene = importer.ReadFileFromMemory( sceneStr.data(), sceneStr.size(), preprocessFlags, format.c_str() );

    if ( !scene )
    {
        return false;
    }

    ImportScene( commandList, *scene, fs::current_path() );

    return true;
}

void Scene::ImportScene( CommandList& commandList, const aiScene& scene, std::filesystem::path parentPath )
{

    if ( m_RootNode )
    {
        m_RootNode.reset();
    }

    m_MaterialMap.clear();
    m_Materials.clear();
    m_Meshes.clear();

    // Import scene materials.
    for ( unsigned int i = 0; i < scene.mNumMaterials; ++i )
    {
        ImportMaterial( commandList, *( scene.mMaterials[i] ), parentPath );
    }
    // Import meshes
    for ( unsigned int i = 0; i < scene.mNumMeshes; ++i )
    {
        ImportMesh( commandList, *( scene.mMeshes[i] ) );
    }

    // Import the root node.
    m_RootNode = ImportSceneNode( commandList, nullptr, scene.mRootNode );
}

void Scene::ImportMaterial( CommandList& commandList, const aiMaterial& material, std::filesystem::path parentPath )
{
    aiString    materialName;
    aiString    aiTexturePath;
    aiTextureOp aiBlendOperation;
    float       blendFactor;
    aiColor4D   diffuseColor;
    aiColor4D   specularColor;
    aiColor4D   ambientColor;
    aiColor4D   emissiveColor;
    float       opacity;
    float       indexOfRefraction;
    float       reflectivity;
    float       shininess;
    float       bumpIntensity;

    std::shared_ptr<Material> pMaterial = std::make_shared<Material>();

    if ( material.Get( AI_MATKEY_COLOR_AMBIENT, ambientColor ) == aiReturn_SUCCESS )
    {
        pMaterial->SetAmbientColor( XMFLOAT4( ambientColor.r, ambientColor.g, ambientColor.b, ambientColor.a ) );
    }
    if ( material.Get( AI_MATKEY_COLOR_EMISSIVE, emissiveColor ) == aiReturn_SUCCESS )
    {
        pMaterial->SetEmissiveColor( XMFLOAT4( emissiveColor.r, emissiveColor.g, emissiveColor.b, emissiveColor.a ) );
    }
    if ( material.Get( AI_MATKEY_COLOR_DIFFUSE, diffuseColor ) == aiReturn_SUCCESS )
    {
        pMaterial->SetDiffuseColor( XMFLOAT4( diffuseColor.r, diffuseColor.g, diffuseColor.b, diffuseColor.a ) );
    }
    if ( material.Get( AI_MATKEY_COLOR_SPECULAR, specularColor ) == aiReturn_SUCCESS )
    {
        pMaterial->SetSpecularColor( XMFLOAT4( specularColor.r, specularColor.g, specularColor.b, specularColor.a ) );
    }
    if ( material.Get( AI_MATKEY_SHININESS, shininess ) == aiReturn_SUCCESS )
    {
        pMaterial->SetSpecularPower( shininess );
    }
    if ( material.Get( AI_MATKEY_OPACITY, opacity ) == aiReturn_SUCCESS )
    {
        pMaterial->SetOpacity( opacity );
    }
    if ( material.Get( AI_MATKEY_REFRACTI, indexOfRefraction ) )
    {
        pMaterial->SetIndexOfRefraction( indexOfRefraction );
    }
    if ( material.Get( AI_MATKEY_REFLECTIVITY, reflectivity ) == aiReturn_SUCCESS )
    {
        pMaterial->SetReflectance( XMFLOAT4( reflectivity, reflectivity, reflectivity, reflectivity ) );
    }
    if ( material.Get( AI_MATKEY_BUMPSCALING, bumpIntensity ) == aiReturn_SUCCESS )
    {
        pMaterial->SetBumpIntensity( bumpIntensity );
    }

    // Load ambient textures.
    if ( material.GetTextureCount( aiTextureType_AMBIENT ) > 0 &&
         material.GetTexture( aiTextureType_AMBIENT, 0, &aiTexturePath, nullptr, nullptr, &blendFactor,
                              &aiBlendOperation ) == aiReturn_SUCCESS )
    {
        fs::path texturePath( aiTexturePath.C_Str() );
        auto     texture = commandList.LoadTextureFromFile( parentPath / texturePath, true );
        pMaterial->SetTexture( Material::TextureType::Ambient, texture );
    }

    // Load emissive textures.
    if ( material.GetTextureCount( aiTextureType_EMISSIVE ) > 0 &&
         material.GetTexture( aiTextureType_EMISSIVE, 0, &aiTexturePath, nullptr, nullptr, &blendFactor,
                              &aiBlendOperation ) == aiReturn_SUCCESS )
    {
        fs::path texturePath( aiTexturePath.C_Str() );
        auto     texture = commandList.LoadTextureFromFile( parentPath / texturePath, true );
        pMaterial->SetTexture( Material::TextureType::Emissive, texture );
    }

    // Load diffuse textures.
    if ( material.GetTextureCount( aiTextureType_DIFFUSE ) > 0 &&
         material.GetTexture( aiTextureType_DIFFUSE, 0, &aiTexturePath, nullptr, nullptr, &blendFactor,
                              &aiBlendOperation ) == aiReturn_SUCCESS )
    {
        fs::path texturePath( aiTexturePath.C_Str() );
        auto     texture = commandList.LoadTextureFromFile( parentPath / texturePath, true );
        pMaterial->SetTexture( Material::TextureType::Diffuse, texture );
    }

    // Load specular texture.
    if ( material.GetTextureCount( aiTextureType_SPECULAR ) > 0 &&
         material.GetTexture( aiTextureType_SPECULAR, 0, &aiTexturePath, nullptr, nullptr, &blendFactor,
                              &aiBlendOperation ) == aiReturn_SUCCESS )
    {
        fs::path texturePath( aiTexturePath.C_Str() );
        auto     texture = commandList.LoadTextureFromFile( parentPath / texturePath, true );
        pMaterial->SetTexture( Material::TextureType::Specular, texture );
    }

    // Load specular power texture.
    if ( material.GetTextureCount( aiTextureType_SHININESS ) > 0 &&
         material.GetTexture( aiTextureType_SHININESS, 0, &aiTexturePath, nullptr, nullptr, &blendFactor,
                              &aiBlendOperation ) == aiReturn_SUCCESS )
    {
        fs::path texturePath( aiTexturePath.C_Str() );
        auto     texture = commandList.LoadTextureFromFile( parentPath / texturePath, false );
        pMaterial->SetTexture( Material::TextureType::SpecularPower, texture );
    }

    if ( material.GetTextureCount( aiTextureType_OPACITY ) > 0 &&
         material.GetTexture( aiTextureType_OPACITY, 0, &aiTexturePath, nullptr, nullptr, &blendFactor,
                              &aiBlendOperation ) == aiReturn_SUCCESS )
    {
        fs::path texturePath( aiTexturePath.C_Str() );
        auto     texture = commandList.LoadTextureFromFile( parentPath / texturePath, false );
        pMaterial->SetTexture( Material::TextureType::Opacity, texture );
    }

    // Load normal map texture.
    if ( material.GetTextureCount( aiTextureType_NORMALS ) > 0 &&
         material.GetTexture( aiTextureType_NORMALS, 0, &aiTexturePath ) == aiReturn_SUCCESS )
    {
        fs::path texturePath( aiTexturePath.C_Str() );
        auto     texture = commandList.LoadTextureFromFile( parentPath / texturePath, false );
        pMaterial->SetTexture( Material::TextureType::Normal, texture );
    }
    // Load bump map (only if there is no normal map).
    else if ( material.GetTextureCount( aiTextureType_HEIGHT ) > 0 &&
              material.GetTexture( aiTextureType_HEIGHT, 0, &aiTexturePath, nullptr, nullptr, &blendFactor ) ==
                  aiReturn_SUCCESS )
    {
        fs::path texturePath( aiTexturePath.C_Str() );
        auto     texture = commandList.LoadTextureFromFile( parentPath / texturePath, false );

        // Some materials actually store normal maps in the bump map slot. Assimp can't tell the difference between
        // these two texture types, so we try to make an assumption about whether the texture is a normal map or a bump
        // map based on its pixel depth. Bump maps are usually 8 BPP (grayscale) and normal maps are usually 24 BPP or
        // higher.
        Material::TextureType textureType =
            ( texture->BitsPerPixel() >= 24 ) ? Material::TextureType::Normal : Material::TextureType::Bump;

        pMaterial->SetTexture( textureType, texture );
    }

    // m_MaterialMap.insert( MaterialMap::value_type( materialName.C_Str(), pMaterial ) );
    m_Materials.push_back( pMaterial );


    // Import data to our own material type
    Material1 newMaterial;
    newMaterial.color = glm::vec3( pMaterial->GetDiffuseColor().x, pMaterial->GetDiffuseColor().y, pMaterial->GetDiffuseColor().z );
    //newMaterial.emittance;
    
    mp_materials.push_back( newMaterial );

}

void Scene::ImportMesh( CommandList& commandList, const aiMesh& aiMesh )
{
    auto mesh = std::make_shared<Mesh>();

    std::vector<VertexPositionNormalTangentBitangentTexture> vertexData( aiMesh.mNumVertices );

    assert( aiMesh.mMaterialIndex < m_Materials.size() );
    mesh->SetMaterial( m_Materials[aiMesh.mMaterialIndex] );

    unsigned int i;
    if ( aiMesh.HasPositions() )
    {
        for ( i = 0; i < aiMesh.mNumVertices; ++i )
        {
            vertexData[i].Position = { aiMesh.mVertices[i].x, aiMesh.mVertices[i].y, aiMesh.mVertices[i].z };
        }
    }

    if ( aiMesh.HasNormals() )
    {
        for ( i = 0; i < aiMesh.mNumVertices; ++i )
        {
            vertexData[i].Normal = { aiMesh.mNormals[i].x, aiMesh.mNormals[i].y, aiMesh.mNormals[i].z };
        }
    }

    if ( aiMesh.HasTangentsAndBitangents() )
    {
        for ( i = 0; i < aiMesh.mNumVertices; ++i )
        {
            vertexData[i].Tangent   = { aiMesh.mTangents[i].x, aiMesh.mTangents[i].y, aiMesh.mTangents[i].z };
            vertexData[i].Bitangent = { aiMesh.mBitangents[i].x, aiMesh.mBitangents[i].y, aiMesh.mBitangents[i].z };
        }
    }

    if ( aiMesh.HasTextureCoords( 0 ) )
    {
        for ( i = 0; i < aiMesh.mNumVertices; ++i )
        {
            vertexData[i].TexCoord = { aiMesh.mTextureCoords[0][i].x, aiMesh.mTextureCoords[0][i].y,
                                       aiMesh.mTextureCoords[0][i].z };
        }
    }

    auto vertexBuffer = commandList.CopyVertexBuffer( vertexData );
    mesh->SetVertexBuffer( 0, vertexBuffer );

    // Extract the index buffer.
    if ( aiMesh.HasFaces() )
    {
        std::vector<unsigned int> indices;
        for ( i = 0; i < aiMesh.mNumFaces; ++i )
        {
            const aiFace& face = aiMesh.mFaces[i];

            Triangle newFace;
            // Only extract triangular faces
            if ( face.mNumIndices == 3 )
            {
                indices.push_back( face.mIndices[0] );
                indices.push_back( face.mIndices[1] );
                indices.push_back( face.mIndices[2] );

                // import data to our own triangle type
                const auto& vertex1Data = vertexData[face.mIndices[0]];
                const auto& vertex2Data = vertexData[face.mIndices[1]];
                const auto& vertex3Data = vertexData[face.mIndices[2]];

                newFace.point1 = glm::vec3( vertex1Data.Position.x, vertex1Data.Position.y, vertex1Data.Position.z );
                newFace.point2 = glm::vec3( vertex2Data.Position.x, vertex2Data.Position.y, vertex2Data.Position.z );
                newFace.point3 = glm::vec3( vertex3Data.Position.x, vertex3Data.Position.y, vertex3Data.Position.z );
                
                newFace.normal1 = glm::vec3( vertex1Data.Normal.x, vertex1Data.Normal.y, vertex1Data.Normal.z );
                newFace.normal2 = glm::vec3( vertex2Data.Normal.x, vertex2Data.Normal.y, vertex2Data.Normal.z );
                newFace.normal3 = glm::vec3( vertex3Data.Normal.x, vertex3Data.Normal.y, vertex3Data.Normal.z );

                newFace.geomId = mp_geoms.size();

                mp_faces.push_back( newFace );
            }
        }

        if ( indices.size() > 0 )
        {
            auto indexBuffer = commandList.CopyIndexBuffer( indices );
            mesh->SetIndexBuffer( indexBuffer );
        }
    }

    // Set the AABB from the AI Mesh's AABB.
    mesh->SetAABB( CreateBoundingBox( aiMesh.mAABB ) );

    m_Meshes.push_back( mesh );

    // import data to our own mesh type
    Geom newGeom;
    newGeom.faceNum = aiMesh.mNumFaces;
    newGeom.faceStartIdx = mp_faces.size() - newGeom.faceNum;
    newGeom.materialid = aiMesh.mMaterialIndex;
    //newGeom.transform;

    mp_geoms.push_back( newGeom );
}

std::shared_ptr<SceneNode> Scene::ImportSceneNode( CommandList& commandList, std::shared_ptr<SceneNode> parent,
                                                   const aiNode* aiNode )
{
    if ( !aiNode )
    {
        return nullptr;
    }

    auto node = std::make_shared<SceneNode>( XMMATRIX( &( aiNode->mTransformation.a1 ) ) );
    node->SetParent( parent );

    if ( aiNode->mName.length > 0 )
    {
        node->SetName( aiNode->mName.C_Str() );
    }
    // Add meshes to scene node
    for ( unsigned int i = 0; i < aiNode->mNumMeshes; ++i )
    {
        assert( aiNode->mMeshes[i] < m_Meshes.size() );

        std::shared_ptr<Mesh> pMesh = m_Meshes[aiNode->mMeshes[i]];
        node->AddMesh( pMesh );

        // import data to our own mesh type
        XMFLOAT4X4 meshTransform;
        XMStoreFloat4x4( &meshTransform, node->GetWorldTransform() ); 
        
        // row major to column major matrix
        mp_geoms[aiNode->mMeshes[i]].transform =
            glm::mat4( meshTransform._11, meshTransform._21, meshTransform._31, meshTransform._41, 
                       meshTransform._12, meshTransform._22, meshTransform._32, meshTransform._42, 
                       meshTransform._13, meshTransform._23, meshTransform._33, meshTransform._43, 
                       meshTransform._14, meshTransform._24, meshTransform._34, meshTransform._44);
    }

    // Recursively Import children
    for ( unsigned int i = 0; i < aiNode->mNumChildren; ++i )
    {
        auto child = ImportSceneNode( commandList, node, aiNode->mChildren[i] );
        node->AddChild( child );
    }

    return node;
}

void Scene::Accept( Visitor& visitor )
{
    visitor.Visit( *this );
    if ( m_RootNode )
    {
        m_RootNode->Accept( visitor );
    }
}

std::vector<Triangle> dx12lib::Scene::getTriangles() const
{
    return mp_faces;
}

DirectX::BoundingBox Scene::GetAABB() const
{
    DirectX::BoundingBox aabb { { 0, 0, 0 }, { 0, 0, 0 } };

    if ( m_RootNode )
    {
        aabb = m_RootNode->GetAABB();
    }

    return aabb;
}
