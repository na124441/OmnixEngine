//Walk The snapshot in the correct order and delegate writing
//Serializer(snapshot) :
//	1. WriteHeader(snapshot.header)
//  2. WriteEntityCount(snapshot.entities.size)
//	3. For each EntitySnapshot(sorted) :
//		WriteEntity(entity)
//	4. Finalize()
//

// ============================================================================
// ECS SNAPSHOT SERIALIZER - Serialize entire ECS snapshot to binary
// ============================================================================

class NormalSerializer {
private:
    // ========== MEMBERS ==========
    // Where to write
    std::ofstream m_FileStream;
    std::vector<uint8_t> m_BufferStream;
    
    // What to write
    const ECSSnapshot* m_Snapshot;
    
    // Status
    std::string m_LastError;
    bool m_IsValid;
    size_t m_BytesWritten;
    
    // ========== PRIVATE HELPERS ==========
    // Low-level writing (primitive types)
    bool WriteUInt32(uint32_t value);
    bool WriteUInt64(uint64_t value);
    bool WriteFloat(float value);
    bool WriteBool(bool value);
    bool WriteBytes(const void* data, size_t size);
    bool WriteString(const std::string& str);
    
    // Mid-level writing (snapshot structures)
    bool WriteHeader(const ECSSnapshotHeader& header);
    bool WriteEntityCount(size_t count);
    bool WriteEntity(const EntitySnapshot& entity);
    bool WriteComponent(const ComponentSnapshot& component);
    bool WriteField(const FieldSnapshot& field);
    
    // Utilities
    bool OpenFile(const std::string& filepath);
    bool CloseFile();
    bool Finalize();

public:
    // ========== PUBLIC API ==========
    NormalSerializer();
    ~NormalSerializer();
    
    // Main entry points
    bool Serialize(const ECSSnapshot& snapshot);
    bool SerializeToFile(const ECSSnapshot& snapshot, 
                        const std::string& filepath);
    bool SerializeToBuffer(const ECSSnapshot& snapshot, 
                          std::vector<uint8_t>& outBuffer);
    
    // Query results
    std::string GetLastError() const;
    size_t GetBytesWritten() const;
    bool WasSuccessful() const;
};
