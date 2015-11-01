#pragma once

#include <queue>

#include "Frame.hh"

namespace BIVCodec
{
  class Encoder
  {
    protected: enum class LimitMatter
    {
      None,
      MSE,
      Length
    };

    protected: LimitMatter limit_matter = LimitMatter::None;
    private: float max_mse = 10;
    private: int max_length = 100;

    protected: bool first_image_enc = true;

    protected: ImageBSP previous_image_bsp{ColorSpace::Grayscale};
    protected: std::vector<Frame> previous_chain;

    protected: std::queue<Frame> frame_stream;


    public: Encoder() = default;

    public: void nextImageMatrix(const ImageMatrix &_mat)
    {
      ImageBSP bsp_source(_mat, ColorSpace::Grayscale);

      auto frame_chain = std::move(bsp_source.asFrameChain());
      auto sync_frame = frame_chain[0];

      std::priority_queue<std::tuple<float, BIVCodec::Frame>> priority_chain;

      if (first_image_enc)
      {
        ImageMatrix black_mat(_mat.width, _mat.height, ColorSpace::Grayscale);
        this->previous_image_bsp.applyFrameFromMatrixRecursive(black_mat,
            Rect(0, 0, black_mat.width, black_mat.height), std::vector<bool>());
        previous_chain = std::move(previous_image_bsp.asFrameChain());
      }

      assert(previous_chain.size() == frame_chain.size());
      for (int i = 1; i < frame_chain.size(); ++i)
      {
        auto a = frame_chain[i].imageData();
        auto b = previous_chain[i].imageData();

        float cost = std::abs(a->value_l-b->value_l)+std::abs(a->value_r-b->value_r);
        cost /= a->location.layer+1;
        priority_chain.push(std::make_tuple(cost, frame_chain[i]));
      }

      frame_chain.clear();
      frame_chain.push_back(sync_frame);
      while (!priority_chain.empty())
      {
        frame_chain.push_back(std::get<1>(priority_chain.top()));
        priority_chain.pop();
      }

      if (!first_image_enc)
      {
        if (this->limit_matter == LimitMatter::Length)
          frame_chain.erase(frame_chain.begin()+this->max_length, frame_chain.end());
      }

      for (auto &item : frame_chain)
        this->frame_stream.push(item);

      this->previous_image_bsp.applyFrameChain(frame_chain);
      previous_chain = std::move(this->previous_image_bsp.asFrameChain());

      if (first_image_enc)
        first_image_enc = false;
    }

    public: bool empty() const
    {
      return this->frame_stream.empty();
    }

    public: void drop()
    {
      while (!this->frame_stream.empty())
        this->frame_stream.pop();
    }

    public: Frame fetchFrame(const bool keep = false)
    {
      auto frame = this->frame_stream.front();

      if (!keep)
        this->frame_stream.pop();

      return frame;
    }

    public: void setMSE(const float _mse)
    {
      this->max_mse = _mse;

      this->limit_matter = LimitMatter::MSE;
    }

    public: float getMSE()
    {
      return this->max_mse;
    }

    public: void limitChainLengthPerImage(const int _num)
    {
      this->max_length = _num;

      this->limit_matter = LimitMatter::Length;
    }
  };
}