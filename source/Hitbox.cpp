#include "Hitbox.h"

namespace InfinideaStudio
{
	namespace NEWorld
	{
		namespace Hitbox
		{

			bool stuck = false;
			AABB Emptybox;

			bool inXclip(const AABB& boxA, const AABB& boxB)
			{
				bool ret = false;
				if(boxA.xmin > boxB.xmin && boxA.xmin<boxB.xmax || boxA.xmax>boxB.xmin && boxA.xmax < boxB.xmax) ret = true;
				if(boxB.xmin > boxA.xmin && boxB.xmin<boxA.xmax || boxB.xmax>boxA.xmin && boxB.xmax < boxA.xmax) ret = true;
				return ret;
			}

			bool inYclip(const AABB& boxA, const AABB& boxB)
			{
				bool ret = false;
				if(boxA.ymin > boxB.ymin && boxA.ymin<boxB.ymax || boxA.ymax>boxB.ymin && boxA.ymax < boxB.ymax) ret = true;
				if(boxB.ymin > boxA.ymin && boxB.ymin<boxA.ymax || boxB.ymax>boxA.ymin && boxB.ymax < boxA.ymax) ret = true;
				return ret;
			}

			bool inZclip(const AABB& boxA, const AABB& boxB)
			{
				bool ret = false;
				if(boxA.zmin > boxB.zmin && boxA.zmin<boxB.zmax || boxA.zmax>boxB.zmin && boxA.zmax < boxB.zmax) ret = true;
				if(boxB.zmin > boxA.zmin && boxB.zmin<boxA.zmax || boxB.zmax>boxA.zmin && boxB.zmax < boxA.zmax) ret = true;
				return ret;
			}

			bool Hit(const AABB& boxA, const AABB& boxB)
			{
				return inXclip(boxA, boxB) && inYclip(boxA, boxB) && inZclip(boxA, boxB);
			}

			double MaxMoveOnXclip(const AABB& boxA, const AABB& boxB, double movedist)
			{
				//用boxA去撞boxB，别搞反了
				double ret = 0.0;
				if(!(inYclip(boxA, boxB) && inZclip(boxA, boxB)))
				{
					ret = movedist;
				}
				else if(boxA.xmin >= boxB.xmax && movedist < 0.00)
				{
					ret = boxB.xmax - boxA.xmin;
					if(ret<movedist) ret = movedist;
				}
				else if(boxA.xmax <= boxB.xmin && movedist > 0.0)
				{
					ret = boxB.xmin - boxA.xmax;
					if(ret > movedist) ret = movedist;
				}
				else
				{
					if(!stuck) ret = movedist;
					else ret = 0.0;
				}
				return ret;
			}

			double MaxMoveOnYclip(const AABB& boxA, const AABB& boxB, double movedist)
			{
				//用boxA去撞boxB，别搞反了 （这好像是句废话）
				double ret = 0.0;
				if(!(inXclip(boxA, boxB) && inZclip(boxA, boxB)))
				{
					ret = movedist;
				}
				else if(boxA.ymin >= boxB.ymax && movedist < 0.00)
				{
					ret = boxB.ymax - boxA.ymin;
					if(ret<movedist) ret = movedist;
				}
				else if(boxA.ymax <= boxB.ymin && movedist > 0.0)
				{
					ret = boxB.ymin - boxA.ymax;
					if(ret > movedist) ret = movedist;
				}
				else
				{
					if(!stuck) ret = movedist;
					else ret = 0.0;
				}
				return ret;
			}

			double MaxMoveOnZclip(const AABB& boxA, const AABB& boxB, double movedist)
			{
				//用boxA去撞boxB，别搞反了 （这好像还是句废话）
				double ret = 0.0;
				if(!(inXclip(boxA, boxB) && inYclip(boxA, boxB)))
				{
					ret = movedist;
				}
				else if(boxA.zmin >= boxB.zmax && movedist < 0.00)
				{
					ret = boxB.zmax - boxA.zmin;
					if(ret<movedist) ret = movedist;
				}
				else if(boxA.zmax <= boxB.zmin && movedist > 0.0)
				{
					ret = boxB.zmin - boxA.zmax;
					if(ret > movedist) ret = movedist;
				}
				else
				{
					if(!stuck) ret = movedist;
					else ret = 0.0;
				}
				return ret;
			}

			AABB Expand(const AABB& box, double xe, double ye, double ze)
			{
				AABB ret = box;
				if(xe > 0.0)
					ret.xmax += xe;
				else
					ret.xmin += xe;

				if(ye > 0.0)
					ret.ymax += ye;
				else
					ret.ymin += ye;

				if(ze > 0.0)
					ret.zmax += ze;
				else
					ret.zmin += ze;

				return ret;
			}

			void Move(AABB &box, double xa, double ya, double za)
			{
				box.xmin += xa;
				box.xmax += xa;
				box.ymin += ya;
				box.ymax += ya;
				box.zmin += za;
				box.zmax += za;
			}

			void MoveTo(AABB &box, double x, double y, double z)
			{
				double l, w, h;
				//注意在执行这个过程时，参数中的xyz坐标将成为移动后的AABB的中心，而不是初始化AABB时的原点！
				l = (box.xmax - box.xmin) / 2;
				w = (box.ymax - box.ymin) / 2;
				h = (box.zmax - box.zmin) / 2;
				box.xmin = x - l;
				box.xmax = x + l;
				box.ymin = y - w;
				box.ymax = y + w;
				box.zmin = z - h;
				box.zmax = z + h;
			}
			void renderAABB(const AABB& box, float colR, float colG, float colB, int mode)
			{
				//Debug only!
				//碰撞箱渲染出来很瞎狗眼的QAQ而且又没用QAQ
				glLineWidth(2.0);
				glEnable(GL_LINE_SMOOTH);
				glColor4f(colR, colG, colB, 1.0);

				if(mode == 1 || mode == 3)
				{
					glBegin(GL_LINE_LOOP);
					glVertex3d(box.xmin, box.ymin, box.zmin);
					glVertex3d(box.xmax, box.ymin, box.zmin);
					glVertex3d(box.xmax, box.ymax, box.zmin);
					glVertex3d(box.xmin, box.ymax, box.zmin);
					glEnd();
					glBegin(GL_LINE_LOOP);
					glVertex3d(box.xmin, box.ymin, box.zmax);
					glVertex3d(box.xmax, box.ymin, box.zmax);
					glVertex3d(box.xmax, box.ymax, box.zmax);
					glVertex3d(box.xmin, box.ymax, box.zmax);
					glEnd();
					glBegin(GL_LINE_LOOP);
					glVertex3d(box.xmin, box.ymin, box.zmin);
					glVertex3d(box.xmax, box.ymin, box.zmin);
					glVertex3d(box.xmax, box.ymin, box.zmax);
					glVertex3d(box.xmin, box.ymin, box.zmax);
					glEnd();
					glBegin(GL_LINE_LOOP);
					glVertex3d(box.xmin, box.ymax, box.zmin);
					glVertex3d(box.xmax, box.ymax, box.zmin);
					glVertex3d(box.xmax, box.ymax, box.zmax);
					glVertex3d(box.xmin, box.ymax, box.zmax);
					glEnd();
					glBegin(GL_LINE_LOOP);
					glVertex3d(box.xmin, box.ymin, box.zmin);
					glVertex3d(box.xmin, box.ymin, box.zmax);
					glVertex3d(box.xmin, box.ymax, box.zmax);
					glVertex3d(box.xmin, box.ymax, box.zmin);
					glEnd();
					glBegin(GL_LINE_LOOP);
					glVertex3d(box.xmax, box.ymin, box.zmin);
					glVertex3d(box.xmax, box.ymin, box.zmax);
					glVertex3d(box.xmax, box.ymax, box.zmax);
					glVertex3d(box.xmax, box.ymax, box.zmin);
					glEnd();
				}

				glColor4f(colR, colG, colB, 0.5);

				if(mode == 2 || mode == 3)
				{
					glBegin(GL_QUADS);
					glVertex3d(box.xmin, box.ymin, box.zmin);
					glVertex3d(box.xmax, box.ymin, box.zmin);
					glVertex3d(box.xmax, box.ymax, box.zmin);
					glVertex3d(box.xmin, box.ymax, box.zmin);
					glVertex3d(box.xmin, box.ymin, box.zmax);
					glVertex3d(box.xmax, box.ymin, box.zmax);
					glVertex3d(box.xmax, box.ymax, box.zmax);
					glVertex3d(box.xmin, box.ymax, box.zmax);
					glVertex3d(box.xmin, box.ymin, box.zmin);
					glVertex3d(box.xmax, box.ymin, box.zmin);
					glVertex3d(box.xmax, box.ymin, box.zmax);
					glVertex3d(box.xmin, box.ymin, box.zmax);
					glVertex3d(box.xmin, box.ymax, box.zmin);
					glVertex3d(box.xmax, box.ymax, box.zmin);
					glVertex3d(box.xmax, box.ymax, box.zmax);
					glVertex3d(box.xmin, box.ymax, box.zmax);
					glVertex3d(box.xmin, box.ymin, box.zmin);
					glVertex3d(box.xmin, box.ymin, box.zmax);
					glVertex3d(box.xmin, box.ymax, box.zmax);
					glVertex3d(box.xmin, box.ymax, box.zmin);
					glVertex3d(box.xmax, box.ymin, box.zmin);
					glVertex3d(box.xmax, box.ymin, box.zmax);
					glVertex3d(box.xmax, box.ymax, box.zmax);
					glVertex3d(box.xmax, box.ymax, box.zmin);
					glEnd();
				}
			}

		}
	}
}