#include "SoundFileReader.hpp"
#include "MayaFlux/Containers/SignalSourceContainer.hpp"
#include <cstring>

namespace MayaFlux::IO {

SoundFileReader::SoundFileReader()
{
    m_sndfile = std::make_unique<SndfileHandle>();
}

SoundFileReader::~SoundFileReader()
{
}

void SoundFileReader::set_file_properties(SF_INFO& file_info)
{
    if (SNDFILE* raw_handle = m_sndfile->rawHandle()) {
        sf_command(raw_handle, SFC_GET_CURRENT_SF_INFO, &file_info, sizeof(SF_INFO));
    } else {
        m_last_error = "Failed to get Sndfile Raw handle";
        throw std::runtime_error(m_last_error);
    }
}

bool SoundFileReader::read_file_to_container(const std::string& file_path, std::shared_ptr<Containers::SignalSourceContainer> container, sf_count_t start_frame, sf_count_t num_frames)
{
    if (!container) {
        m_last_error = "Invalid container (null)";
        return false;
    }

    std::vector<double> samples;
    SF_INFO file_info = {};

    if (!read_file_to_memory(file_path, samples, file_info, start_frame, num_frames)) {
        return false;
    }

    std::vector<std::pair<std::string, uint64_t>> markers;
    std::unordered_map<std::string, Containers::RegionGroup> region_groups;
    extract_file_metadata(file_path, markers, region_groups);

    u_int64_t total_frames = file_info.frames;

    container->setup(total_frames, file_info.samplerate, file_info.channels);
    container->set_raw_samples(samples);

    if (!markers.empty()) {
        Containers::RegionGroup markers_group { "markers" };
        for (const auto& marker : markers) {
            if (marker.second < total_frames) {
                Containers::RegionPoint point {
                    marker.second,
                    marker.second
                };
                point.point_attributes["label"] = marker.first;
                markers_group.points.push_back(point);
            }
        }
        if (!markers_group.points.empty()) {
            container->add_region_group(markers_group);
        }
    }

    for (const auto& [group_name, group] : region_groups) {
        container->add_region_group(group);
    }

    if (file_info.channels > 1) {
        container->set_interleaved(true);
    } else {
        container->set_interleaved(false);
    }

    container->create_default_processor();

    return true;
}

bool SoundFileReader::read_file_to_memory(const std::string& file_path, std::vector<double>& samples, SF_INFO& file_info, sf_count_t start_frame, sf_count_t num_frames)
{
    samples.clear();
    m_sndfile = std::make_unique<SndfileHandle>(file_path, SFM_READ);

    if (!m_sndfile || m_sndfile->error()) {
        m_last_error = std::format("Error opening file {}: {}", file_path, m_sndfile->strError());
        return false;
    }

    set_file_properties(file_info);

    if (start_frame < 0 || start_frame >= m_sndfile->frames()) {
        m_last_error = std::format("Invalid start frame: {}", start_frame);
        return false;
    }

    sf_count_t frames_to_read = num_frames;
    if (frames_to_read < 0 || start_frame + frames_to_read > m_sndfile->frames()) {
        frames_to_read = m_sndfile->frames() - start_frame;
    }

    if (start_frame > 0) {
        m_sndfile->seek(start_frame, SEEK_SET);
    }

    samples.resize(frames_to_read * m_sndfile->channels());

    sf_count_t frames_read = m_sndfile->readf(samples.data(), frames_to_read);
    if (frames_read != frames_to_read) {
        m_last_error = std::format("Error reading file {}: expected {} frames but read {}",
            file_path, frames_to_read, frames_read);
        return false;
    }

    file_info.frames = frames_read;

    std::cout << "Read " << frames_read << " frames, "
              << samples.size() << " total samples" << std::endl;

    return true;
}

std::string SoundFileReader::get_last_error() const
{
    return m_last_error;
}

void SoundFileReader::close_file()
{
    m_sndfile = std::make_unique<SndfileHandle>();
}

bool SoundFileReader::extract_file_metadata(
    const std::string& file_path,
    std::vector<std::pair<std::string, uint64_t>>& markers,
    std::unordered_map<std::string, Containers::RegionGroup>& region_groups)
{
    markers.clear();
    region_groups.clear();
    bool need_to_open_file = !m_sndfile.get() || !m_sndfile->rawHandle();

    if (need_to_open_file) {
        SF_INFO temp_info = SF_INFO {};
        m_sndfile = std::make_unique<SndfileHandle>(file_path, SFM_READ);

        if (!m_sndfile->rawHandle()) {
            m_last_error = std::format("Error opening file for metadata extraction: {}", m_sndfile->strError());
            return false;
        }

        set_file_properties(temp_info);
    }

    bool success = extract_markers(markers) || extract_region_groups(region_groups);

    if (need_to_open_file) {
        close_file();
    }

    return success;
}

bool SoundFileReader::extract_markers(std::vector<std::pair<std::string, u_int64_t>>& markers)
{
    if (!m_sndfile || !m_sndfile->rawHandle()) {
        return false;
    }

    bool found_markers = false;
    SNDFILE* raw_handle = m_sndfile->rawHandle();

    SF_CUES cues;
    memset(&cues, 0, sizeof(cues));

    if (sf_command(raw_handle, SFC_GET_CUE, &cues, sizeof(cues)) == SF_TRUE) {
        for (size_t i = 0; i < cues.cue_count; ++i) {
            SF_CUE_POINT cue = cues.cue_points[i];

            std::string name;
            if (cue.indx > 0) {
                name = std::format("Cue_{}", cue.indx);
            } else {
                name = std::format("Cue_{}", i + 1);
            }

            markers.emplace_back(name, cue.sample_offset);
        }

        found_markers = cues.cue_count > 0;
    }

    SF_BROADCAST_INFO binfo;
    memset(&binfo, 0, sizeof(binfo));

    if (sf_command(raw_handle, SFC_GET_BROADCAST_INFO, &binfo, sizeof(binfo)) == SF_TRUE) {
        if (binfo.time_reference_high != 0 || binfo.time_reference_low != 0) {
            uint64_t time_ref = (static_cast<uint64_t>(binfo.time_reference_high) << 32)
                | binfo.time_reference_low;
            markers.emplace_back("BWF_TimeRef", time_ref);
            found_markers = true;
        }

        if (binfo.description[0] != '\0') {
            for (int i = 0; i < 256; i++) {
                if (binfo.description[i] == '\0') {
                    if (i > 0) {
                        std::string desc(binfo.description, i);
                        markers.emplace_back("BWF_Description", 0);
                    }
                    break;
                }
            }
        }

        if (binfo.originator[0] != '\0') {
            std::string orig(binfo.originator, strnlen(binfo.originator, 32));
            markers.emplace_back("BWF_Originator", 0);
        }

        if (binfo.origination_date[0] != '\0') {
            std::string date(binfo.origination_date, strnlen(binfo.origination_date, 10));
            markers.emplace_back("BWF_Date", 0);
        }
    }
    return found_markers;
}

bool SoundFileReader::extract_region_groups(std::unordered_map<std::string, Containers::RegionGroup>& region_groups)
{
    if (!m_sndfile || !m_sndfile->rawHandle()) {
        return false;
    }

    SF_CUES* cues = nullptr;
    SF_INSTRUMENT inst;
    bool found_regions = false;

    Containers::RegionGroup cue_points { "cue_points" };
    Containers::RegionGroup loops { "loops" };
    Containers::RegionGroup markers { "markers" };

    if (sf_command(m_sndfile->rawHandle(), SFC_GET_CUE, &cues, sizeof(cues)) == SF_TRUE) {
        for (int i = 0; i < cues->cue_count; i++) {
            Containers::RegionPoint point {
                static_cast<uint64_t>(cues->cue_points[i].position),
                static_cast<uint64_t>(cues->cue_points[i].position)
            };
            point.point_attributes["label"] = std::string("cue_" + std::to_string(i));
            cue_points.points.push_back(point);
            found_regions = true;
        }
    }

    if (sf_command(m_sndfile->rawHandle(), SFC_GET_INSTRUMENT, &inst, sizeof(inst)) == SF_TRUE) {
        for (int i = 0; i < inst.loop_count; i++) {
            Containers::RegionPoint point {
                static_cast<uint64_t>(inst.loops[i].start),
                static_cast<uint64_t>(inst.loops[i].end)
            };
            point.point_attributes["mode"] = inst.loops[i].mode;
            point.point_attributes["count"] = inst.loops[i].count;
            loops.points.push_back(point);
            found_regions = true;
        }
    }

    SF_BROADCAST_INFO binfo;
    memset(&binfo, 0, sizeof(binfo));

    if (sf_command(m_sndfile->rawHandle(), SFC_GET_BROADCAST_INFO, &binfo, sizeof(binfo)) == SF_TRUE) {
        if (binfo.time_reference_high != 0 || binfo.time_reference_low != 0) {
            uint64_t time_ref = (static_cast<uint64_t>(binfo.time_reference_high) << 32)
                | binfo.time_reference_low;
            Containers::RegionPoint time_ref_point {
                time_ref,
                time_ref
            };
            time_ref_point.point_attributes["type"] = "time_reference";
            markers.points.push_back(time_ref_point);
            found_regions = true;
        }

        if (binfo.description[0] != '\0') {
            std::string desc(binfo.description, strnlen(binfo.description, 256));
            Containers::RegionPoint desc_point { 0, 0 };
            desc_point.point_attributes["type"] = "description";
            desc_point.point_attributes["value"] = desc;
            markers.points.push_back(desc_point);
        }
        if (binfo.originator[0] != '\0') {
            std::string orig(binfo.originator, strnlen(binfo.originator, 32));
            Containers::RegionPoint orig_point { 0, 0 };
            orig_point.point_attributes["type"] = "originator";
            orig_point.point_attributes["value"] = orig;
            markers.points.push_back(orig_point);
        }
    }

    if (!cue_points.points.empty()) {
        region_groups["cue_points"] = cue_points;
    }
    if (!loops.points.empty()) {
        region_groups["loops"] = loops;
    }
    if (!markers.points.empty()) {
        region_groups["markers"] = markers;
    }

    return found_regions;
}

}
