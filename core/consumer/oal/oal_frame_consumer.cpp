/*
* copyright (c) 2010 Sveriges Television AB <info@casparcg.com>
*
*  This file is part of CasparCG.
*
*    CasparCG is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    CasparCG is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.

*    You should have received a copy of the GNU General Public License
*    along with CasparCG.  If not, see <http://www.gnu.org/licenses/>.
*
*/
 
#include "../../StdAfx.h"

#include "oal_frame_consumer.h"

#include "../../frame/gpu_frame.h"
#include "../../frame/frame_format.h"

#include <SFML/Audio.hpp>

#include <boost/circular_buffer.hpp>
#include <boost/foreach.hpp>

#include <windows.h>

namespace caspar{ namespace audio{	

class sound_channel : public sf::SoundStream
{
public:
	sound_channel() : internal_chunks_(5), silence_(1920*2, 0)
	{
		external_chunks_.set_capacity(3);
		sf::SoundStream::Initialize(2, 48000);
	}

	~sound_channel()
	{
		external_chunks_.clear();
		external_chunks_.push(nullptr);
		Stop();
	}
	
	void push(const gpu_frame_ptr& frame)
	{
		if(frame->audio_data().empty())
			frame->audio_data() = silence_;

		//if(!external_chunks_.try_push(frame))
		//{
			//CASPAR_LOG(debug) << "Sound Buffer Overrun";
			external_chunks_.push(frame);
		//}

		if(GetStatus() != Playing && external_chunks_.size() >= 3)
			Play();
	}

private:

	bool OnStart() 
	{
		external_chunks_.clear();
		return true;
	}

	bool OnGetData(sf::SoundStream::Chunk& data)
    {
		gpu_frame_ptr frame;
		//if(!external_chunks_.try_pop(frame))
		//{
			//CASPAR_LOG(trace) << "Sound Buffer Underrun";
			external_chunks_.pop(frame);
		//}

		if(frame == nullptr)
		{
			external_chunks_.clear();
			return false;
		}

		internal_chunks_.push_back(frame);
		//SetVolume(pChunk->volume());
		data.Samples = reinterpret_cast<sf::Int16*>(frame->audio_data().data());
		data.NbSamples = frame->audio_data().size();
        return true;
    }

	std::vector<short> silence_;
	boost::circular_buffer<gpu_frame_ptr> internal_chunks_;
	tbb::concurrent_bounded_queue<gpu_frame_ptr> external_chunks_;
};
typedef std::shared_ptr<sound_channel> sound_channelPtr;

struct oal_frame_consumer::implementation : boost::noncopyable
{	
	implementation(const frame_format_desc& format_desc) : format_desc_(format_desc)
	{
	}
	
	void push(const gpu_frame_ptr& frame)
	{
		channel_.push(frame);
	}
		
	sound_channel channel_;

	caspar::frame_format_desc format_desc_;
};

oal_frame_consumer::oal_frame_consumer(const caspar::frame_format_desc& format_desc) : impl_(new implementation(format_desc)){}
const caspar::frame_format_desc& oal_frame_consumer::get_frame_format_desc() const{return impl_->format_desc_;}
void oal_frame_consumer::prepare(const gpu_frame_ptr& frame){impl_->push(frame);}
}}
