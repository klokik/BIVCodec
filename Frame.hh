#include <cassert>
#include <ctime>

#include <algorithm>
#include <vector>
#include <tuple>
#include <memory>
#include <map>


namespace BIVCodec
{
  // To keep PoC simple enough, support Grayscale images only;
  enum class ColorSpace
  {
    Grayscale,
    HSL,
    RGB
  };

  struct FrameLocation
  {
    std::vector<bool> path;
    int layer;
    int location_id;

    bool Path(const int _layer) const noexcept
    {
      assert(_layer <= layer);
      return path[_layer];
    }
  };

  struct FrameHeader
  {
    enum class HeaderType {Image,Sync};
    HeaderType type;
  };

  struct FrameData
  { };

  struct FrameSyncData: public FrameData
  {
    // CodecType codec_type;
    // CodecVersion codec_version;

    using FrameSourceWidth = int;
    using FrameSourceRatio = float;
    using FrameID = int;
    using Timestamp = uint32_t;

    FrameSourceWidth width;
    FrameSourceRatio ratio;

    ColorSpace color_format;

    FrameID id;
    Timestamp timestamp;
  };

  struct FrameImageData: public FrameData
  {
    FrameLocation location;
    int channel;
    float value_l;
    float value_r;
  };

  struct Frame
  {
    FrameHeader header;
    std::shared_ptr<FrameData> data;
  };

  struct Rect
  {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    constexpr Rect() = default;

    constexpr Rect(const int _x, const int _y, const int _width, const int _height):
        x(_x), y(_y), width(_width), height(_height)
    { }

    constexpr bool isHorizontal()
    { return this->width > this->height; }

    constexpr bool isVertical()
    { return !this->isHorizontal(); }
  };

  std::pair<Rect, Rect> splitRect(const Rect _rect)
  {
    if (_rect.isHorizontal())
      return std::make_pair(
          Rect(_rect.x,
               _rect.y,
               _rect.width/2,
               _rect.height),
          Rect(_rect.x+_rect.width/2,
               _rect.y,
               _rect.width-(_rect.width/2),
               _rect.height));
    else
      return std::make_pair(
          Rect(_rect.x,
               _rect.y,
               _rect.width,
               _rect.height/2),
          Rect(_rect.x,
               _rect.y+_rect.height/2,
               _rect.width,
               _rect.height-(_rect.height/2)));
  }

  class ImageMatrix
  {
    public: int width;
    public: int height;
    private: ColorSpace color_mode;

    private: std::vector<float> image_data;

    public: ImageMatrix(ImageMatrix &&_src):
        width(_src.width), height(_src.height), color_mode(_src.color_mode),
        image_data(std::move(_src.image_data))
    { }

    public: ImageMatrix(const int _width, const int _height,
        const ColorSpace _mode = ColorSpace::Grayscale, const void *_data = nullptr):
        width(_width), height(_height), color_mode(_mode),
        image_data(_width*_height, 0.f)
    {
      if (_data)
      {
        const uint8_t *data = reinterpret_cast<const uint8_t *>(_data);
        for (size_t i = 0; i < _width*_height; ++i)
          this->image_data[i] = data[i];
      }
    }

    public: ImageMatrix &operator=(const ImageMatrix &_src)
    {
      this->width = _src.width;
      this->height = _src.height;
      this->color_mode = _src.color_mode;

      this->image_data = _src.image_data;

      return *this;
    }

    public: ImageMatrix &operator=(const ImageMatrix &&_src)
    {
      this->width = _src.width;
      this->height = _src.height;
      this->color_mode = _src.color_mode;

      this->image_data = std::move(_src.image_data);

      return *this;
    }

    public: inline float getFragment(const int _x, const int _y, const int _channel = 0) const noexcept
    {
      assert((_x >= 0) && (_x < this->width));
      assert((_y >= 0) && (_y < this->height));

      return this->image_data[_y*this->width + _x];
    }

    public: inline float getFragment(const int _id) const noexcept
    {
      assert((_id >= 0) && (_id < this->width*this->height));

      return this->image_data[_id];
    }

    public: inline void setFragment(const int _x, const int _y, const int _channel, const float _value) noexcept
    {
      assert((_x >= 0) && (_x < this->width));
      assert((_y >= 0) && (_y < this->height));

      this->image_data[_y*this->width + _x] = _value;
    }

    public: inline void setFragment(const int _id, const float _value) noexcept
    {
      assert((_id >= 0) && (_id < this->width*this->height));

      this->image_data[_id] = _value;
    }

    public: float getAverageValue(const Rect &_roi) const
    {
      float acc = 0;

      for (int i = 0; i < _roi.width; ++i)
        for (int j = 0; j < _roi.height; ++j)
          acc += this->getFragment(_roi.x+i, _roi.y+j, 0);

      return acc/(_roi.width*_roi.height);
    }

    public: void fillRect(const Rect &_roi, const float _value) noexcept
    {
      for (int i = 0; i < _roi.width; ++i)
        for (int j = 0; j < _roi.height; ++j)
          this->setFragment(_roi.x+i, _roi.y+j, 0, _value);
    }
  };


  class ImageBSP
  {
    private: float width = 1.0f;
    private: float ratio = 1.0f;
    private: ColorSpace color_mode;

    // TODO: Make private
    public: class ImageNode
    {
      public: float value = 0.f;
      public: int layer = 0;

      public: std::weak_ptr<ImageNode> parent;

      public: std::shared_ptr<ImageNode> left;
      public: std::shared_ptr<ImageNode> right;

      public: ImageNode(const float _value, const int _layer):
          value(_value), layer(_layer)
      { }
    };

    private: std::shared_ptr<ImageNode> root_node = std::make_shared<ImageNode>(0.f,0);

    public: int frames = 0;

    public: explicit ImageBSP(const ColorSpace _mode):
        color_mode(_mode)
    { }

    public: ImageBSP(const ImageMatrix &_src, const ColorSpace _mode)
    {
      assert(_src.width >= 2);
      assert(_src.height >= 1);

      this->width = _src.width;
      this->ratio = static_cast<float>(_src.height)/_src.width;
      this->color_mode = ColorSpace::Grayscale;

      this->applyFrameFromMatrixRecursive(_src, Rect(0, 0, _src.width, _src.height), std::vector<bool>());
    }

    /// SINGLETHREAD
    protected: float applyFrameFromMatrixRecursive(const ImageMatrix &_src, const Rect &_roi, std::vector<bool> _path)
    {
      // limit number of layers, (24bit path depth - 4k resolution max)
      if ((std::max(_roi.width,_roi.height) <= 1) || (_path.size() > 24))
      {
        return _src.getAverageValue(_roi);
      }

      FrameImageData fdata;
      fdata.location.path = _path;
      fdata.location.layer = _path.size();
      fdata.location.location_id = -1;                    // use unique counter

      Rect rect_left;
      Rect rect_right;

      std::tie(rect_left, rect_right) = splitRect(_roi);

      fdata.channel = 0;

      auto path_left = _path;
      auto path_right = _path;

      path_left.push_back(0);
      path_right.push_back(1);

      // thread it?
      fdata.value_l = applyFrameFromMatrixRecursive(_src, rect_left, path_left);
      fdata.value_r = applyFrameFromMatrixRecursive(_src, rect_right, path_right);

      applyFrameData(fdata);

      return (fdata.value_l+fdata.value_r)/2;
    }

    public: ImageMatrix asImageMatrix(const int _width) const noexcept
    {
      int _height = _width*this->ratio;
      ImageMatrix image(_width, _height, ColorSpace::Grayscale);

      applyNodeToMatrixRecursive(image, Rect(0, 0, _width, _height), this->root_node);

      return std::move(image);
    }

    protected: void applyNodeToMatrixRecursive(ImageMatrix &_dst, const Rect &_roi, std::shared_ptr<ImageNode> _node) const noexcept
    {
      if((!_node->left) && (!_node->right))
      {
        _dst.fillRect(_roi, _node->value);
        return;
      }

      if (std::max(_roi.width,_roi.height) <= 1)
        return;

      Rect rect_left;
      Rect rect_right;

      std::tie(rect_left, rect_right) = splitRect(_roi);

      if (_node->left)
        applyNodeToMatrixRecursive(_dst, rect_left, _node->left);
      else
        _dst.fillRect(rect_left, _node->value);

      if (_node->right)
        applyNodeToMatrixRecursive(_dst, rect_right, _node->right);
      else
        _dst.fillRect(rect_right, _node->value);
    }

    /// THREAD UNSAFE
    public: std::weak_ptr<ImageNode> applyFrameData(const FrameImageData &_modifier) noexcept
    {
      auto curr_node = this->root_node;

      while (_modifier.location.layer != curr_node->layer)
      {
        if (!_modifier.location.path[curr_node->layer])
        {
          if (!curr_node->left)
          {
            curr_node->left = std::make_shared<ImageNode>(-1.f, curr_node->layer+1);
            curr_node->left->parent = curr_node;
          }

          curr_node = curr_node->left;
          continue;
        }
        else
        {
          if (!curr_node->right)
          {
            curr_node->right = std::make_shared<ImageNode>(-1.f, curr_node->layer+1);
            curr_node->right->parent = curr_node;
          }

          curr_node = curr_node->right;
          continue;
        }
      }

      // FIXME biolerplate code
      if (!curr_node->left)
      {
        curr_node->left = std::make_shared<ImageNode>(-1.f, curr_node->layer+1);
        curr_node->left->parent = curr_node;
      }
      if (!curr_node->right)
      {
        curr_node->right = std::make_shared<ImageNode>(-1.f, curr_node->layer+1);
        curr_node->right->parent = curr_node;
      }

      curr_node->left->value = _modifier.value_l;
      curr_node->right->value = _modifier.value_r;

      this->frames++;

      return curr_node;
    }

    public: std::vector<Frame> asFrameChain() noexcept
    {
      std::vector<Frame> frame_chain;
      std::map<int,std::vector<Frame>> layers;

      Frame frame;

      frame.header.type = FrameHeader::HeaderType::Sync;

      auto sync_data = std::make_shared<FrameSyncData>();
      frame.data = std::static_pointer_cast<FrameData>(sync_data);

      sync_data->width = this->width;
      sync_data->ratio = this->ratio;

      sync_data->color_format = this->color_mode;
      sync_data->id = -1;

      sync_data->timestamp = static_cast<uint32_t>(std::time(nullptr));

      frame_chain.push_back(frame);

      std::function<void(std::shared_ptr<ImageNode>)> pushNodeRecursive;
      pushNodeRecursive = [&layers, &frame, &pushNodeRecursive](std::shared_ptr<ImageNode> _node)
      {
        auto image_data = std::make_shared<FrameImageData>();
        frame.data = std::static_pointer_cast<FrameData>(image_data);

        image_data->channel = 0;

        image_data->value_l = _node->left ? _node->left->value : _node->value;
        image_data->value_r = _node->right ? _node->right->value : _node->value;

        layers[_node->layer].push_back(frame);

        if (_node->left)
          pushNodeRecursive(_node->left);

        if (_node->right)
          pushNodeRecursive(_node->right);
      };

      pushNodeRecursive(this->root_node);

      for (auto layer : layers)
      {
        /// TODO: Permutate elements in each layer
        frame_chain.insert(frame_chain.end(),layer.second.begin(),layer.second.end());
      }

      return std::move(frame_chain);
    }
  };
};