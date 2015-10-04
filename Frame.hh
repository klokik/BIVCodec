#include <cassert>

#include <algorithm>
#include <vector>
#include <tuple>
#include <memory>


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
    enum HeaderType {Image,Sync};
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
    using Timestamp = int;

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
    FrameData *data;
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

  /// UNTESTED
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

    public: ImageMatrix(const int _width, const int _height, const ColorSpace _mode = ColorSpace::Grayscale):
        width(_width), height(_height), color_mode(_mode),
        image_data(_width*_height, 0.f)
    { }

    public: inline float getFragment(const int _x, const int _y, const int _channel = 0) const noexcept
    { return 0; /* DUMMY */ }

    public: float getAverageValue(const Rect &_roi) const
    { return 0; /* DUMMY */ }

    public: void fillRect(const Rect &_roi, const float value) noexcept
    { return; /* DUMMY */ }
  };


  class ImageBSP
  {
    private: float width = 1.0f;
    private: float ratio = 1.0f;
    private: ColorSpace color_mode;

    private: class ImageNode
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

    /// UNTESTED
    public: ImageBSP(const ImageMatrix &_src, const ColorSpace _mode)
    {
      assert(_src.width >= 2);
      assert(_src.height >= 1);

      this->ratio = static_cast<float>(_src.height)/_src.width;
      this->color_mode = ColorSpace::Grayscale;

      this->applyFrameFromMatrixRecursive(_src, Rect(0, 0, _src.width, _src.height), std::vector<bool>());
    }

    /// UNTESTED
    /// SINGLETHREAD
    protected: void applyFrameFromMatrixRecursive(const ImageMatrix &_src, const Rect &_roi, std::vector<bool> _path)
    {
      // limit number of layers, (24bit path depth - 4k resolution max)
      if ((std::max(_roi.width,_roi.height) <= 1) || (_path.size() > 24))
        return;

      FrameImageData fdata;
      fdata.location.path = _path;
      fdata.location.layer = _path.size();
      fdata.location.location_id = -1;                    // use unique counter

      Rect rect_left;
      Rect rect_right;

      std::tie(rect_left, rect_right) = splitRect(_roi);

      fdata.channel = 0;

      fdata.value_l = _src.getAverageValue(rect_left);
      fdata.value_r = _src.getAverageValue(rect_right);

      applyFrameData(fdata);

      auto path_left = _path;
      auto path_right = _path;

      path_left.push_back(0);
      path_right.push_back(1);

      // thread it?
      applyFrameFromMatrixRecursive(_src, rect_left, path_left);
      applyFrameFromMatrixRecursive(_src, rect_right, path_right);
    }

    /// UNTESTED
    /// NOT OPTIMAL
    public: ImageMatrix asImageMatrix(const int _width) const noexcept
    {
      int _height = _width*this->ratio;
      ImageMatrix image(_width, _height, ColorSpace::Grayscale);

      applyNodeToMatrixRecursive(image, Rect(0, 0, _width, _height), this->root_node);
    }

    /// UNTESTED
    /// NOT OPTIMAL
    protected: void applyNodeToMatrixRecursive(ImageMatrix &_dst, const Rect &_roi, std::shared_ptr<ImageNode> _node) const noexcept
    {

      Rect rect_left;
      Rect rect_right;

      _dst.fillRect(_roi, _node->value);

      if (std::max(_roi.width,_roi.height) <= 1)
        return;

      std::tie(rect_left, rect_right) = splitRect(_roi);

      if (_node->left)
        applyNodeToMatrixRecursive(_dst, rect_left, _node->left);
      else
        applyNodeToMatrixRecursive(_dst, rect_right, _node->right);
    }

    /// UNTESTED
    /// THREAD UNSAFE
    public: void applyFrameData(const FrameImageData &_modifier) noexcept
    {
      auto curr_node = this->root_node;

      while (_modifier.location.layer != curr_node->layer)
      {
        if (!_modifier.location.Path(curr_node->layer))
        {
          if (!curr_node->left)
            curr_node->left = std::make_shared<ImageNode>(-1.f, curr_node->layer+1);

          curr_node = curr_node->left;
          continue;
        }
        else
        {
          if (!curr_node->right)
            curr_node->right = std::make_shared<ImageNode>(-1.f, curr_node->layer+1);

          curr_node = curr_node->right;
          continue;
        }
      }

      // FIXME biolerplate code
      if (!curr_node->left)
        curr_node->left = std::make_shared<ImageNode>(-1.f, curr_node->layer+1);
      if (!curr_node->right)
        curr_node->right = std::make_shared<ImageNode>(-1.f, curr_node->layer+1);

      curr_node->left->value = _modifier.value_l;
      curr_node->right->value = _modifier.value_r;

      this->frames++;
    }
  };
};