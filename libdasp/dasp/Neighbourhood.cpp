/*
 * Neighbourhood.cpp
 *
 *  Created on: May 18, 2012
 *      Author: david
 */

#include "Neighbourhood.hpp"
#include <Slimage/Convert.hpp>

namespace dasp
{

std::vector<unsigned int> ComputeAllBorderPixels(const Superpixels& superpixels)
{
	slimage::Image1i labels = superpixels.ComputeLabels();
	std::set<unsigned int> u;
	int face_neighbours[] = { -1, +1, -labels.width(), +labels.width() };
	for(unsigned int y=1; y<labels.height()-1; y++) {
		for(unsigned int x=1; x<labels.width()-1; x++) {
			int q = labels.index(x,y);
			unsigned int lc = labels[q];
			for(unsigned int i=0; i<4; i++) {
				if(labels[q + face_neighbours[i]] != lc) {
					u.insert(q);
				}
			}
		}
	}
	return std::vector<unsigned int>(u.begin(), u.end());
}

//std::vector<unsigned int> ComputeBorderPixels(const std::vector<unsigned int>& pixel_ids, unsigned int desired_neighbour_cid, const slimage::Image1i& labels)
//{
//	const int w = static_cast<int>(labels.width());
//	const int h = static_cast<int>(labels.height());
//	const int d[4] = { -1, +1, -w, +w };
//	std::vector<unsigned int> border;
//	for(unsigned int pid : pixel_ids) {
//		int x = pid % w;
//		int y = pid / w;
//		if(1 <= x && x+1 <= w && 1 <= y && y+1 <= h) {
//			for(int i=0; i<4; i++) {
//				int label = labels[pid + d[i]];
//				if(label == static_cast<int>(desired_neighbour_cid)) {
//					border.push_back(pid);
//				}
//			}
//		}
//	}
//	return border;
//}

struct BorderPixel
{
	unsigned int pixel_id;
	unsigned int label;
};

std::vector<BorderPixel> ComputeBorderLabels(unsigned int cid, const Superpixels& spc, const slimage::Image1i& labels)
{
	const int w = static_cast<int>(labels.width());
	const int h = static_cast<int>(labels.height());
	const int d[4] = { -1, +1, -w, +w };
	std::vector<BorderPixel> border;
	for(unsigned int pid : spc.cluster[cid].pixel_ids) {
		int x = pid % w;
		int y = pid / w;
		if(1 <= x && x+1 <= w && 1 <= y && y+1 <= h) {
			for(int i=0; i<4; i++) {
				int label = labels[pid + d[i]];
				if(label != cid && label != -1) {
					border.push_back(BorderPixel{pid, label});
				}
			}
		}
	}
	return border;
}

std::vector<std::vector<BorderPixel>> ComputeBorderLabels(const Superpixels& spc)
{
	slimage::Image1i labels = spc.ComputeLabels();
	std::vector<std::vector<BorderPixel>> border_pixels(spc.cluster.size());
	for(unsigned int cid=0; cid<spc.cluster.size(); cid++) {
		border_pixels[cid] = ComputeBorderLabels(cid, spc, labels);
	}
	return border_pixels;
}

std::vector<unsigned int> FindCommonBorder(const std::vector<std::vector<BorderPixel>>& border, unsigned int cid, unsigned cjd) {
	const std::vector<BorderPixel>& bi = border[cid];
	const std::vector<BorderPixel>& bj = border[cjd];
	std::vector<unsigned int> common;
	common.reserve(bi.size() + bj.size());
	for(const BorderPixel& q : bi) {
		if(q.label == cjd) {
			common.push_back(q.pixel_id);
		}
	}
	for(const BorderPixel& q : bi) {
		if(q.label == cjd) {
			common.push_back(q.pixel_id);
		}
	}
	return common;
}

BorderPixelGraph CreateNeighborhoodGraph(const Superpixels& superpixels, NeighborGraphSettings settings)
{
	// create one node for each superpixel
	BorderPixelGraph neighbourhood_graph = detail::CreateSuperpixelGraph<BorderPixelGraph>(superpixels.clusterCount());
	// compute superpixel borders
	std::vector<std::vector<BorderPixel> > border = ComputeBorderLabels(superpixels);
	// connect superpixels
	const float node_distance_threshold = settings.max_spatial_distance_mult * superpixels.opt.base_radius;
	for(unsigned int i=0; i<superpixels.cluster.size(); i++) {
		for(unsigned int j=i+1; j<superpixels.cluster.size(); j++) {
			if(settings.cut_by_spatial) {
				float d = (superpixels.cluster[i].center.world - superpixels.cluster[j].center.world).norm();
				// only test if distance is smaller than threshold
				if(d > node_distance_threshold) {
					continue;
				}
			}
			// compute intersection of border pixels
			std::vector<unsigned int> common_border = FindCommonBorder(border, i, j);
			// test if superpixels have a common border
			unsigned int common_border_size = common_border.size();
			if(common_border_size < settings.min_abs_border_overlap) {
				continue;
			}
			float p = static_cast<float>(common_border_size) / static_cast<float>(std::min(border[i].size(), border[j].size()));
			if(p < settings.min_border_overlap) {
				continue;
			}
			// add edge
			BorderPixelGraph::edge_descriptor eid;
			bool ok;
			boost::tie(eid,ok) = boost::add_edge(i, j, neighbourhood_graph); // FIXME correctly convert superpixel_id to vertex descriptor
			assert(ok);
			boost::put(borderpixels_t(), neighbourhood_graph, eid, common_border);
		}
	}
	return neighbourhood_graph;
}

slimage::Image1ub CreateSmoothedContourImage(const slimage::Image1f& src, float scl)
{
	slimage::Image1f tmp(src.dimensions(), slimage::Pixel1f{1.0f});
	for(unsigned int x=1; x<tmp.width()-1; x++) {
		for(unsigned int y=1; y<tmp.height()-1; y++) {
			float nb = src(x-1,y) + src(x+1,y) + src(x,y-1) + src(x,y+1);
			float v = src(x,y) + nb / 4.0f;
			tmp(x,y) = 1.0f - scl * v;
		}
	}
	slimage::Image1ub vis(tmp.dimensions());
	slimage::conversion::Convert(tmp, vis);
	return vis;
}

}