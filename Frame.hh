#pragma once

#include <cassert>
#include <ctime>
#include <iostream>

#include <algorithm>
#include <random>
#include <future>
#include <tuple>
#include <memory>
#include <map>
#include <vector>

#define DEF_THREAD_NUM 2

namespace BIVCodec
{
  // To keep PoC simple enough, support Grayscale images only;
  enum class ColorSpace : int
  {
    Grayscale = 0,
    HSL = 1,
    RGB = 2
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

    uint32_t fuse() const noexcept
    {
      uint32_t fusion = 0;

      for (int i = 0; i < path.size(); ++i)
      {
        if (path[i])
          fusion |= 1u<<i;
      }

      return fusion;
    }

    void defuse(const uint32_t _fusion, const int _layer) noexcept
    {
      layer = _layer;

      path.clear();
      for (int i = 0; i < layer; ++i)
        path.push_back(_fusion & (1u<<i));
    }

    bool operator==(const FrameLocation &_a)
    {
      return path == _a.path;
    }
  };

  struct FrameHeader
  {
    enum class HeaderType : int {Image = 0, Sync = 1};
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

    bool operator==(const FrameSyncData &_a)
    {
      return width == _a.width;
    }
  };

  struct FrameImageData: public FrameData
  {
    FrameLocation location;
    int channel;
    float value_l;
    float value_r;

    bool operator==(const FrameImageData &_a)
    {
      return (location == _a.location) &&
             (channel == _a.channel) &&
             (std::abs(value_l-_a.value_l) < 1.01) &&
             (std::abs(value_r-_a.value_r) < 1.01);
    }
  };

  struct Frame
  {
    FrameHeader header;
    std::shared_ptr<FrameData> data;

    std::vector<uint8_t> serialize()
    {
      std::vector<uint8_t> binary_data;

      binary_data.push_back(static_cast<uint8_t>(header.type));

      if (header.type == FrameHeader::HeaderType::Image)
      {
        auto img = std::static_pointer_cast<FrameImageData>(data);

        binary_data.push_back(img->location.layer);
        uint32_t path = img->location.fuse();
        binary_data.push_back(path);
        binary_data.push_back(path>>8);
        binary_data.push_back(path>>16);
        binary_data.push_back(static_cast<uint8_t>(img->channel));
        binary_data.push_back(static_cast<uint8_t>(img->value_l));
        binary_data.push_back(static_cast<uint8_t>(img->value_r));
      }
      else
      {
        auto sync = std::static_pointer_cast<FrameSyncData>(data);

        binary_data.push_back(sync->width%256);
        binary_data.push_back(sync->width/256);
        binary_data.push_back(sync->ratio*128);
        binary_data.push_back(static_cast<uint8_t>(sync->color_format));
        binary_data.push_back(sync->id);
        binary_data.push_back(sync->timestamp%256);
        binary_data.push_back(sync->timestamp/256);
      }

      return std::move(binary_data);
    }

    void deserialize(const uint8_t *_data)
    {
      assert(_data != nullptr);

      header.type = static_cast<FrameHeader::HeaderType>(_data[0]);

      if (header.type == FrameHeader::HeaderType::Image)
      {
        std::shared_ptr<FrameImageData> img;

        if (!data)
        {
          img = std::make_shared<FrameImageData>();
          data = std::static_pointer_cast<FrameData>(img);
        }
        else
          img = std::static_pointer_cast<FrameImageData>(data);

        img->location.defuse(
          static_cast<uint32_t>(_data[2])|
          static_cast<uint32_t>(_data[3])<<8|
          static_cast<uint32_t>(_data[4])<<16,
          _data[1]);
        img->channel = _data[5];
        img->value_l = _data[6];
        img->value_r = _data[7];
      }
      else
      {
        std::shared_ptr<FrameSyncData> sync;

        if (!data)
        {
          sync = std::make_shared<FrameSyncData>();
          data = std::static_pointer_cast<FrameData>(sync);
        }
        else
          sync = std::static_pointer_cast<FrameSyncData>(data);

        sync->width = _data[1]|(_data[2]<<8);
        sync->ratio = _data[3]/128.f;
        sync->color_format = static_cast<ColorSpace>(_data[4]);
        sync->id = _data[5];
        sync->timestamp = _data[6]|(_data[7]<<8);
      }
    }

    bool operator==(const Frame &_a)
    {
      bool eq = (header.type == _a.header.type);

      if (eq)
      {
        if (header.type == FrameHeader::HeaderType::Image)
          eq &= (*std::static_pointer_cast<FrameImageData>(data) == *std::static_pointer_cast<FrameImageData>(_a.data));
        else
          eq &= (*std::static_pointer_cast<FrameSyncData>(data) == *std::static_pointer_cast<FrameSyncData>(_a.data));
      }

      return eq;
    }

    // DUMMY
    bool operator<(const Frame &) const
    { return false; }

    void dump()
    {
      std::cout << "{'header_type:";

      if (header.type == FrameHeader::HeaderType::Image)
      {
        auto img = std::static_pointer_cast<FrameImageData>(data);

        std::cout << "'image',"
                  << "'chann':" << img->channel
                  << ",'val_l':" << img->value_l
                  << ",'val_r':" << img->value_r
                  << ",'location':{'layer':" << img->location.layer
                  << ",'path':[";

        for (auto item : img->location.path)
          std::cout << "'" << item << "',";
        std::cout << "\b]}";

        std::cout << "}" << std::endl;
      }
      else
      {
        auto sync = std::static_pointer_cast<FrameSyncData>(data);

        std::cout << "'sync',..." << "}" << std::endl;
      }
    }

    std::shared_ptr<FrameImageData> imageData()
    {
      assert(header.type == FrameHeader::HeaderType::Image);
      return std::static_pointer_cast<FrameImageData>(data);
    }

    std::shared_ptr<FrameSyncData> syncData()
    {
      assert(header.type == FrameHeader::HeaderType::Sync);
      return std::static_pointer_cast<FrameSyncData>(data);
    }
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

    constexpr bool isHorizontal() const noexcept
    { return this->width > this->height; }

    constexpr bool isVertical() const noexcept
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

    public: float *data() noexcept
    {
      return &this->image_data[0];
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

  ImageMatrix matrixMap(const ImageMatrix &_a, std::function<float(float)> _fun)
  {
    ImageMatrix _b(_a.width, _a.height, ColorSpace::Grayscale);

    for (int i = 0; i < _b.width*_b.height; ++i)
      _b.setFragment(i, _fun(_a.getFragment(i)));

    return std::move(_b);
  }

  ImageMatrix matrixMap2(const ImageMatrix &_a, const ImageMatrix &_b, std::function<float(float,float)> _fun)
  {
    assert(_a.width == _b.width);
    assert(_a.height == _b.height);

    ImageMatrix _c(_a.width, _a.height, ColorSpace::Grayscale);

    for (int i = 0; i < _b.width*_b.height; ++i)
      _c.setFragment(i, _fun(_a.getFragment(i), _b.getFragment(i)));

    return std::move(_c);
  }

  class ImageBSP
  {
    private: float width = 1.0f;
    private: float ratio = 1.0f;
    private: ColorSpace color_mode;

    private: const float empty_color = -1.f;

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

    private: std::shared_ptr<ImageNode> root_node = std::make_shared<ImageNode>(this->empty_color,0);

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

      this->applyFrameFromMatrixRecursive(_src, Rect(0, 0, _src.width, _src.height), std::vector<bool>(), DEF_THREAD_NUM);
    }

    /// PROTECTED
    public: float applyFrameFromMatrixRecursive(const ImageMatrix &_src, const Rect &_roi, std::vector<bool> _path, int _threads = 1)
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

      if (_threads == 1)
      {
        fdata.value_l = applyFrameFromMatrixRecursive(_src, rect_left, std::move(path_left));
        fdata.value_r = applyFrameFromMatrixRecursive(_src, rect_right, std::move(path_right));
      }
      else
      {
        auto future_l = std::async(std::launch::async,
            [this](const ImageMatrix &_a, const Rect &_b, std::vector<bool> &_c, int _d) -> float
                { return applyFrameFromMatrixRecursive(_a, _b, std::move(_c), _d); },
            std::ref(_src), std::ref(rect_left), std::ref(path_left), _threads/2);

        fdata.value_r = applyFrameFromMatrixRecursive(_src, rect_right, std::move(path_right), (_threads-_threads/2));

        fdata.value_l = future_l.get();
      }

      applyFrameData(fdata);

      return (fdata.value_l+fdata.value_r)/2;
    }

    public: ImageMatrix asImageMatrix(const int _width) const noexcept
    {
      int _height = _width*this->ratio;
      ImageMatrix image(_width, _height, ColorSpace::Grayscale);

      applyNodeToMatrixRecursive(image, Rect(0, 0, _width, _height), this->root_node, DEF_THREAD_NUM);

      return std::move(image);
    }

    protected: void applyNodeToMatrixRecursive(ImageMatrix &_dst, const Rect &_roi, std::shared_ptr<ImageNode> _node, int _threads = 1) const noexcept
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

      auto applyLeft = [this, &_node, &rect_left, &_dst](int _th) {
        if (_node->left)
          applyNodeToMatrixRecursive(_dst, rect_left, _node->left, _th);
        else
          _dst.fillRect(rect_left, _node->value);
      };

      auto applyRight = [this, &_node, &rect_right, &_dst](int _th) {
        if (_node->right)
          applyNodeToMatrixRecursive(_dst, rect_right, _node->right, _th);
        else
          _dst.fillRect(rect_right, _node->value);
      };

      if (_threads == 1)
      {
        applyLeft(1);
        applyRight(1);
      }
      else
      {
        std::thread left_thread(applyLeft, _threads/2);
        applyRight(_threads - (_threads/2));

        left_thread.join();
      }
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
            curr_node->left = std::make_shared<ImageNode>(this->empty_color, curr_node->layer+1);
            curr_node->left->parent = curr_node;
          }

          curr_node = curr_node->left;
          continue;
        }
        else
        {
          if (!curr_node->right)
          {
            curr_node->right = std::make_shared<ImageNode>(this->empty_color, curr_node->layer+1);
            curr_node->right->parent = curr_node;
          }

          curr_node = curr_node->right;
          continue;
        }
      }

      // FIXME biolerplate code
      if (!curr_node->left)
      {
        curr_node->left = std::make_shared<ImageNode>(this->empty_color, curr_node->layer+1);
        curr_node->left->parent = curr_node;
      }
      if (!curr_node->right)
      {
        curr_node->right = std::make_shared<ImageNode>(this->empty_color, curr_node->layer+1);
        curr_node->right->parent = curr_node;
      }

      curr_node->left->value = _modifier.value_l;
      curr_node->right->value = _modifier.value_r;

      curr_node->value = (_modifier.value_l+_modifier.value_r)/2;

      this->frames++;

      return curr_node;
    }

    public: void applyFrameData(const FrameSyncData &_modifier) noexcept
    {
      this->width = _modifier.width;
      this->ratio = _modifier.ratio;
      this->color_mode = _modifier.color_format;
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

      std::function<void(std::shared_ptr<ImageNode>,  std::vector<bool>)> pushNodeRecursive;
      pushNodeRecursive = [&layers, &frame, &pushNodeRecursive](std::shared_ptr<ImageNode> _node, std::vector<bool> _path)
      {
        if ((!_node->left) || (!_node->right))
          return;

        auto image_data = std::make_shared<FrameImageData>();
        frame.data = std::static_pointer_cast<FrameData>(image_data);

        image_data->location.layer = _path.size();
        image_data->location.path = _path;
        image_data->location.location_id = -1;         // TODO unique id

        image_data->channel = 0;

        image_data->value_l = _node->left ? _node->left->value : _node->value;
        image_data->value_r = _node->right ? _node->right->value : _node->value;

        layers[_node->layer].push_back(frame);


        if (_node->left)
        {
          auto new_path = _path;
          new_path.push_back(0);
          pushNodeRecursive(_node->left, std::move(new_path));
        }

        if (_node->right)
        {
          auto new_path = _path;
          new_path.push_back(1);
          pushNodeRecursive(_node->right, std::move(new_path));
        }
      };

      frame.header.type = FrameHeader::HeaderType::Image;

      pushNodeRecursive(this->root_node, std::vector<bool>());

      for (auto layer : layers)
      {
        /// TODO integrate one layer into another

        // use deterministic permutations
        std::mt19937 re(0);

        std::shuffle(layer.second.begin(), layer.second.end(), re);

        frame_chain.insert(frame_chain.end(), layer.second.begin(), layer.second.end());
      }

      return std::move(frame_chain);
    }

    public: void applyFrameChain(const std::vector<Frame> &_frames) noexcept
    {
      for (auto frame : _frames)
      {
        if (frame.header.type == FrameHeader::HeaderType::Image)
          applyFrameData(*std::static_pointer_cast<FrameImageData>(frame.data));
        else
          applyFrameData(*std::static_pointer_cast<FrameSyncData>(frame.data));
      }
    }

    public: void repair()
    {
      this->repairNodeValueRecursive(this->root_node);
    }

    protected: float repairNodeValueRecursive(std::shared_ptr<ImageNode> _node) noexcept
    {
      if (_node->left && _node->right)
      {
        _node->value = (repairNodeValueRecursive(_node->left)+repairNodeValueRecursive(_node->right))/2;
      }
      else
      {
        if (_node->left)
        {
          float child_value = repairNodeValueRecursive(_node->left);
          if (_node->value == this->empty_color)
            _node->value = child_value;
          else
            _node->right = std::make_shared<ImageNode>(_node->value*2-child_value, _node->layer+1);
        }
        else
        {
          if (_node->right)
          {
            float child_value = repairNodeValueRecursive(_node->right);
            if (_node->value == this->empty_color)
              _node->value = child_value;
            else
              _node->left = std::make_shared<ImageNode>(_node->value*2-child_value, _node->layer+1);
          }
        }
      }

      return _node->value;
    }
  };
};