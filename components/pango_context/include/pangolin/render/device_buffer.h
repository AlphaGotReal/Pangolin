#include <pangolin/utils/shared.h>
#include <sophus/image/runtime_image.h>

namespace pangolin
{

using RuntimePixelType = sophus::RuntimePixelType;

struct DeviceBuffer : std::enable_shared_from_this<DeviceBuffer>
{
    // For now, just channels x width x height
    static constexpr size_t kMaxDims = 3;

    enum class Kind
    {
        Texture,
        VertexIndices,
        VertexAttributes,
        // ... add more
    };

    struct Update
    {
        // Source and destination types must be the same
        RuntimePixelType data_type;

        std::shared_ptr<const uint8_t> src_data;
        std::array<size_t,kMaxDims> src_sizes = {};
        std::array<size_t,kMaxDims> src_pitches_bytes = {};

        std::array<size_t,kMaxDims> dest_pos = {};
        std::array<size_t,kMaxDims> dest_sizes = {};
    };

    // Schedule update of all or a region of this device buffer
    // from source data. Will return immediately under usual policy
    // Thread-safe. Doesn't require graphics context.
    virtual void update(const Update& update) = 0;

    // Force upload of queued updates in this thread.
    // Most implementations will require a current graphics context
    // is available.
    virtual void sync() = 0;

    // Returns true if this object is uninitialized and contains
    // no data or typed information
    virtual bool empty() = 0;

    /////////////////////////////////////////////////////////
    // Convenience template methods for type inference

    struct GiveParams
    {
        std::array<size_t,kMaxDims> dest_pos = {};
    };

    // Use move semantics to transfer ownership of vec for upload
    // Buffer update occurs asynchronously according to policy.
    // Returns immediately.
    template<typename T>
    void give(std::vector<T>&& vec, GiveParams p, int length = 0, int start = 0)
    {
        // Default to entire vector
        if(length==0) length = vec.size() - start;
        FARM_CHECK(start + length <= vec.size());
        if(length==0) return; // noop

        // Move into refcounted wrapper. This will fall out of scope on return,
        // but .src_data will use a linked shared_ptr which will keep the data alive.
        auto svec = std::make_shared<std::vector<T>>(std::move(vec));

        // Call into implementation with type erasure
        update({
            .data_type = RuntimePixelType::fromTemplate<T>(),
            .src_data = std::shared_ptr<const uint8_t>(svec, static_cast<const uint8_t*>(svec.data())),
            .src_pitches_bytes = {sizeof(T)},
            .src_sizes = {length},
            .dest_pos = p.dest_pos
        });
    }

    // Use image's shared ownership semantics to provide a ref-counted pointer
    // Buffer update occurs asynchronously according to policy.
    // Returns immediately.
    void give(const sophus::RuntimeImage<>& image, GiveParams p)
    {
        update({
            .data_type = image.pixelType(),
            .src_data = image.ptr(),
            .src_pitches_bytes = {(size_t)image.pixelType().num_channels, image.shape().pitchBytes()},
            .src_sizes = { (size_t)image.shape().width(), (size_t)image.shape().height()},
            .dest_pos = p.dest_pos
        });
    }

    /////////////////////////////////////////////////////////
    // Factory

    struct Params {
        Kind kind;
    };
    static Shared<DeviceBuffer> Create(Params p);

    virtual ~DeviceBuffer() {}
};


}
