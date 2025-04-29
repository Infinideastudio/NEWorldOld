#include "Particles.h"
#include "World.h"
#include "Textures.h"
#include "Renderer.h"

namespace Particles {
	std::vector<Particle> ptcs;
	int ptcsrendered;
	double pxpos, pypos, pzpos;

	void update(Particle &ptc) {
		double dx, dy, dz;
		float psz = ptc.psize;

		ptc.hb.xmin = ptc.xpos - psz;
		ptc.hb.xmax = ptc.xpos + psz;
		ptc.hb.ymin = ptc.ypos - psz;
		ptc.hb.ymax = ptc.ypos + psz;
		ptc.hb.zmin = ptc.zpos - psz;
		ptc.hb.zmax = ptc.zpos + psz;

		ptc.ysp -= 0.01f;
		dx = ptc.xsp;
		dy = ptc.ysp;
		dz = ptc.zsp;

		vector<Hitbox::AABB> Hitboxes = World::getHitboxes(Hitbox::Expand(ptc.hb, dx, dy, dz));
		size_t hitnum = Hitboxes.size();
		for (size_t i = 0; i < hitnum; i++){
			dy = Hitbox::MaxMoveOnYclip(ptc.hb, Hitboxes[i], dy);
		}
		Hitbox::Move(ptc.hb, 0.0, dy, 0.0);
		for (size_t i = 0; i < hitnum; i++){
			dx = Hitbox::MaxMoveOnXclip(ptc.hb, Hitboxes[i], dx);
		}
		Hitbox::Move(ptc.hb, dx, 0.0, 0.0);
		for (size_t i = 0; i < hitnum; i++){
			dz = Hitbox::MaxMoveOnZclip(ptc.hb, Hitboxes[i], dz);
		}
		Hitbox::Move(ptc.hb, 0.0, 0.0, dz);

		ptc.xpos += dx;
		ptc.ypos += dy;
		ptc.zpos += dz;
		ptc.xsp *= 0.6f;
		ptc.zsp *= 0.6f;
		if (dy != ptc.ysp) ptc.ysp = 0.0;
		ptc.lasts -= 1;
	}

	void updateall(){
		std::vector<Particle> next;
		for (auto& ptc: ptcs){
			update(ptc);
			if (ptc.lasts > 0) next.emplace_back(std::move(ptc));
		}
		ptcs = std::move(next);
	}

	void mesh(Particle const& ptc, double interp) {
		float psize = ptc.psize;
		float bl = static_cast<float>(ptc.bl);
		float tex = static_cast<float>(ptc.tex);
		float tcx = ptc.tcx;
		float tcy = ptc.tcy;
		// float palpha = (ptc.lasts < 30 ? ptc.lasts / 30.0f : 1.0f);
		float xpos = static_cast<float>(ptc.xpos - ptc.xsp + ptc.xsp * interp - pxpos);
		float ypos = static_cast<float>(ptc.ypos - ptc.ysp + ptc.ysp * interp - pypos);
		float zpos = static_cast<float>(ptc.zpos - ptc.zsp + ptc.zsp * interp - pzpos);

		float col = World::getBrightness(RoundInt(ptc.xpos), RoundInt(ptc.ypos), RoundInt(ptc.zpos)) / (float)World::MaxBrightness;
		float col1 = col, col2 = col;
		if (!Renderer::AdvancedRender) {
			col1 *= 0.5f;
			col2 *= 0.7f;
		}
		Renderer::TexCoord3f(0.0f, 0.0f, tex);

		if (Renderer::AdvancedRender) {
			Renderer::Normal3f(1.0f, 0.0f, 0.0f);
			Renderer::Attrib1f(bl);
		}
		Renderer::Color3f(col2, col2, col2);
		Renderer::TexCoord2f(tcx, tcy); Renderer::Vertex3f(xpos + psize, ypos + psize, zpos - psize);
		Renderer::TexCoord2f(tcx + psize, tcy); Renderer::Vertex3f(xpos + psize, ypos + psize, zpos + psize);
		Renderer::TexCoord2f(tcx + psize, tcy + psize); Renderer::Vertex3f(xpos + psize, ypos - psize, zpos + psize);
		Renderer::TexCoord2f(tcx, tcy + psize); Renderer::Vertex3f(xpos + psize, ypos - psize, zpos - psize);

		if (Renderer::AdvancedRender) {
			Renderer::Normal3f(-1.0f, 0.0f, 0.0f);
			Renderer::Attrib1f(bl);
		}
		Renderer::Color3f(col2, col2, col2);
		Renderer::TexCoord2f(tcx, tcy); Renderer::Vertex3f(xpos - psize, ypos - psize, zpos - psize);
		Renderer::TexCoord2f(tcx + psize, tcy); Renderer::Vertex3f(xpos - psize, ypos - psize, zpos + psize);
		Renderer::TexCoord2f(tcx + psize, tcy + psize); Renderer::Vertex3f(xpos - psize, ypos + psize, zpos + psize);
		Renderer::TexCoord2f(tcx, tcy + psize); Renderer::Vertex3f(xpos - psize, ypos + psize, zpos - psize);

		if (Renderer::AdvancedRender) {
			Renderer::Normal3f(0.0f, 1.0f, 0.0f);
			Renderer::Attrib1f(bl);
		}
		Renderer::Color3f(col, col, col);
		Renderer::TexCoord2f(tcx, tcy); Renderer::Vertex3f(xpos + psize, ypos + psize, zpos - psize);
		Renderer::TexCoord2f(tcx + psize, tcy); Renderer::Vertex3f(xpos - psize, ypos + psize, zpos - psize);
		Renderer::TexCoord2f(tcx + psize, tcy + psize); Renderer::Vertex3f(xpos - psize, ypos + psize, zpos + psize);
		Renderer::TexCoord2f(tcx, tcy + psize); Renderer::Vertex3f(xpos + psize, ypos + psize, zpos + psize);

		if (Renderer::AdvancedRender) {
			Renderer::Normal3f(0.0f, -1.0f, 0.0f);
			Renderer::Attrib1f(bl);
		}
		Renderer::Color3f(col, col, col);
		Renderer::TexCoord2f(tcx, tcy); Renderer::Vertex3f(xpos - psize, ypos - psize, zpos - psize);
		Renderer::TexCoord2f(tcx + psize, tcy); Renderer::Vertex3f(xpos + psize, ypos - psize, zpos - psize);
		Renderer::TexCoord2f(tcx + psize, tcy + psize); Renderer::Vertex3f(xpos + psize, ypos - psize, zpos + psize);
		Renderer::TexCoord2f(tcx, tcy + psize); Renderer::Vertex3f(xpos - psize, ypos - psize, zpos + psize);

		if (Renderer::AdvancedRender) {
			Renderer::Normal3f(0.0f, 0.0f, 1.0f);
			Renderer::Attrib1f(bl);
		}
		Renderer::Color3f(col1, col1, col1);
		Renderer::TexCoord2f(tcx, tcy); Renderer::Vertex3f(xpos - psize, ypos - psize, zpos + psize);
		Renderer::TexCoord2f(tcx + psize, tcy); Renderer::Vertex3f(xpos + psize, ypos - psize, zpos + psize);
		Renderer::TexCoord2f(tcx + psize, tcy + psize); Renderer::Vertex3f(xpos + psize, ypos + psize, zpos + psize);
		Renderer::TexCoord2f(tcx, tcy + psize); Renderer::Vertex3f(xpos - psize, ypos + psize, zpos + psize);

		if (Renderer::AdvancedRender) {
			Renderer::Normal3f(0.0f, 0.0f, -1.0f);
			Renderer::Attrib1f(bl);
		}
		Renderer::Color3f(col1, col1, col1);
		Renderer::TexCoord2f(tcx, tcy); Renderer::Vertex3f(xpos - psize, ypos + psize, zpos - psize);
		Renderer::TexCoord2f(tcx + psize, tcy); Renderer::Vertex3f(xpos + psize, ypos + psize, zpos - psize);
		Renderer::TexCoord2f(tcx + psize, tcy + psize); Renderer::Vertex3f(xpos + psize, ypos - psize, zpos - psize);
		Renderer::TexCoord2f(tcx, tcy + psize); Renderer::Vertex3f(xpos - psize, ypos - psize, zpos - psize);
	}

	void renderall(double xpos, double ypos, double zpos, double interp) {
		pxpos = xpos; pypos = ypos; pzpos = zpos;
		ptcsrendered = 0;

		if (Renderer::AdvancedRender) Renderer::Begin(GL_QUADS, 3, 3, 1, 3, 1);
		else Renderer::Begin(GL_QUADS, 3, 3, 1);
		for (auto const& ptc: ptcs) {
			mesh(ptc, interp);
			ptcsrendered++;
		}
		Renderer::End().render();
	}

	void throwParticle(BlockID pt, float x, float y, float z, float xs, float ys, float zs, float psz, int last){
		Particle ptc;
		ptc.xpos = x;
		ptc.ypos = y;
		ptc.zpos = z;
		ptc.xsp = xs;
		ptc.ysp = ys;
		ptc.zsp = zs;
		ptc.psize = psz;
		ptc.hb.xmin = x - psz;
		ptc.hb.xmax = x + psz;
		ptc.hb.ymin = y - psz;
		ptc.hb.ymax = y + psz;
		ptc.hb.zmin = z - psz;
		ptc.hb.zmax = z + psz;
		ptc.lasts = last;
		ptc.bl = pt;
		ptc.tex = Textures::getTextureIndex(pt, 2);
		ptc.tcx = static_cast<float>(rnd()) * (1.0f - psz);
		ptc.tcy = static_cast<float>(rnd()) * (1.0f - psz);
		ptcs.emplace_back(std::move(ptc));
	}
}
