#pragma once
#include "SDL.h"
#include "xxlib_mpobject.h"
#include <functional>
#include <cassert>

namespace xxlib
{
	inline static void Loop(std::function<bool()> cb, std::function<bool(SDL_Event&)> eventHandler = nullptr)
	{
		SDL_Event event;
		do
		{
			while (SDL_PollEvent(&event))
			{
				if (event.type == SDL_QUIT) return;
				if (eventHandler && !eventHandler(event)) return;
				//if (event.type == SDL_KEYDOWN) { switch( event.key.keysym.sym ) { case SDLK_UP: case SDLK_DOWN: default:; }
			}
		} while (cb());
	}


	// todo: Node ������Ȳ�ʵ����. Ҫ����������������ӵ�λ��. ��ǰ���Ե��� sprite ����������ϷԪ������Ľṹ�͹���.

	struct Texture;
	// ӳ�䵽��ͼ��ĳ���������Ƭ��ʾ��λ
	struct SpriteFrame : public MPObject
	{
		Texture* texture;
		SDL_Rect rect;				// λ�� texture �е�ӳ������
		SpriteFrame(Texture* texture, SDL_Rect const& rect);
		SpriteFrame(Texture* texture);
		~SpriteFrame();
	};

	// ��ͼ��ĳ����������Ⱦ�����ӳ��
	struct Sprite : public MPObject
	{
		SpriteFrame* sframe;
		SDL_Point pos;				// ê��������ʾ
		SDL_Point siz;				// ��Ⱦ�ɶ��
		float angle;				// �Ƕ�
		float apX, apY;				// ê��λ�������λ��
		float scaleX, scaleY;		// 2�������� siz ֮��. Ĭ��ֵΪ 1

		bool dirty;					// �Ĺ���������Ժ���Ҫ�Լ������Ա����¼���

		SDL_Rect calc_rect;			// ������( ������Ⱦ )
		SDL_Point calc_pos;			// ������( ������Ⱦ )

		void Calc()
		{
			auto w = siz.x * scaleX;
			auto h = siz.y * scaleY;
			calc_rect.w = int(w);
			calc_rect.h = int(h);
			calc_rect.x = pos.x - (calc_rect.w >> 1);
			calc_rect.y = pos.y - (calc_rect.h >> 1);
			calc_pos.x = int(w * apX);
			calc_pos.y = int(h * apY);
			dirty = false;
		}

		void SetPos(SDL_Point const& pos)
		{
			if (this->pos.x != pos.x || this->pos.y != pos.y)
			{
				this->pos = pos;
				dirty = true;
			}
		}
		void SetSiz(SDL_Point const& siz)
		{
			if (this->siz.x != siz.x || this->siz.y != siz.y)
			{
				this->siz = siz;
				dirty = true;
			}
		}
		void SetAngle(float angle)
		{
			if (this->angle != angle)
			{
				this->angle = angle;
				dirty = true;
			}
		}
		void SetScaleX(float scaleX)
		{
			if (this->scaleX != scaleX)
			{
				this->scaleX = scaleX;
				dirty = true;
			}
		}
		void SetScaleY(float scaleY)
		{
			if (this->scaleY != scaleY)
			{
				this->scaleY = scaleY;
				dirty = true;
			}
		}
		void SetScale(float scaleX, float scaleY)
		{
			SetScaleX(scaleX);
			SetScaleX(scaleY);
		}

		Sprite(SpriteFrame* sframe)
			: sframe(sframe)
			, pos(SDL_Point{ 0, 0 })
			, siz(SDL_Point{ sframe->rect.w, sframe->rect.h })
			, angle(0.0f)
			, apX(0.5f)
			, apY(0.5f)
			, scaleX(1.0f)
			, scaleY(1.0f)
		{
			Calc();
			sframe->AddRef();
		}
		~Sprite()
		{
			sframe->Release();
		}
	};

	// ��ͼ
	struct Texture : public MPObject
	{
		SDL_Renderer* sdl_renderer = nullptr;
		SDL_Texture* sdl_texture = nullptr;
		int w = 0, h = 0;	// �� surface ��������ԭʼ���
		Texture(SDL_Renderer* r) : sdl_renderer(r) {}
		bool Init(char const* fn)
		{
			assert(!sdl_texture);
			auto surface = SDL_LoadBMP(fn);
			if (!surface) return false;
			ScopeGuard sg([&] { SDL_FreeSurface(surface); });
			SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGB(surface->format, 0xFF, 0xFF, 0xFF));	// �ٵ�͸��ɫ( BMP ��ʽû��͸��ͨ�� )
			sdl_texture = SDL_CreateTextureFromSurface(sdl_renderer, surface);
			if (!sdl_texture) return false;
			w = surface->w;
			h = surface->h;
			return true;
		}
		~Texture()
		{
			if (refCount()) return;
			SDL_DestroyTexture(sdl_texture);
		}
	};

	SpriteFrame::SpriteFrame(Texture* texture, SDL_Rect const& rect)
		: texture(texture)
		, rect(rect)
	{
		texture->AddRef();
	}
	SpriteFrame::SpriteFrame(Texture* texture)
		: SpriteFrame(texture, SDL_Rect{ 0, 0, texture->w, texture->h })
	{}
	SpriteFrame::~SpriteFrame()
	{
		texture->Release();
	}


	// һ�����׵� SDL ������. ����һ�� Window ����. ֻ���Դ���һ������
	struct Window
	{
		MemPool mp;
		int w = 0, h = 0;
		bool inited = false;
		SDL_Window* sdl_window = nullptr;
		SDL_Renderer* sdl_renderer = nullptr;
		ListEx<Texture*>* textures = nullptr;
		ListEx<SpriteFrame*>* spriteFrames = nullptr;
		LinksEx<Sprite*>* sprites = nullptr;				// ��Ҫ��ʾ�����ľ���ͼ��. ���������˸��ǻ���˳��

		Window()
		{
			mp.CreateTo(textures);
			mp.CreateTo(sprites);
			mp.CreateTo(spriteFrames);
		}
		~Window()
		{
			for (auto& o : *sprites) sprites->At(o)->Release();
			for (auto& o : *spriteFrames) o->Release();
			for (auto& o : *textures) o->Release();
			sprites->Release();
			spriteFrames->Release();
			textures->Release();
			if (sdl_renderer) SDL_DestroyRenderer(sdl_renderer);
			if (sdl_window) SDL_DestroyWindow(sdl_window);
			if (inited) SDL_Quit();
		}
		bool Init(char const *title, int w, int h, int x = SDL_WINDOWPOS_UNDEFINED, int y = SDL_WINDOWPOS_UNDEFINED, Uint32 flags = SDL_WINDOW_SHOWN)
		{
			if (SDL_Init(SDL_INIT_EVERYTHING)) return false;
			inited = true;
			SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");					// texture filtering to linear
			sdl_window = SDL_CreateWindow(title, x, y, w, h, flags);
			if (!sdl_window) return false;
			sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
			if (!sdl_renderer) return false;
			SDL_SetRenderDrawColor(sdl_renderer, 0xFF, 0xFF, 0xFF, 0xFF);
			this->w = w;
			this->h = h;
			return true;
		}
		void SetTitle(char const* title)
		{
			SDL_SetWindowTitle(sdl_window, title);
		}

		// ����, �洢 ������ ��ͼָ��
		Texture* CreateAndStoreTexture(char const* fn)
		{
			auto t = mp.Create<Texture>(sdl_renderer);
			if (!t->Init(fn))
			{
				t->Release();
				return nullptr;
			}
			textures->Add(t);
			return t;
		}

		// ����, �洢 ������ ����֡ ָ��
		template<typename ...Args>
		SpriteFrame* CreateAndStoreSpriteFrame(Args&&...args)
		{
			auto sframe = mp.Create<SpriteFrame>(std::forward<Args>(args)...);
			spriteFrames->Add(sframe);
			return sframe;
		}

		// ���� ���� ������ָ��. ��Ҫ�Լ� Push �� sprites �Կ���˳��
		template<typename ...Args>
		Sprite* CreateSprite(Args&&...args)
		{
			return mp.Create<Sprite>(std::forward<Args>(args)...);
		}

		void Render()
		{
			SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, 0xFF);
			SDL_RenderClear(sdl_renderer);
			for (auto&i : *sprites)
			{
				auto& spr = sprites->At(i);
				if (spr->dirty) spr->Calc();
				if (spr->angle == 0.0f)
				{
					SDL_RenderCopy(sdl_renderer, spr->sframe->texture->sdl_texture, &spr->sframe->rect, &spr->calc_rect);
				}
				else
				{
					SDL_RenderCopyEx(sdl_renderer, spr->sframe->texture->sdl_texture, &spr->sframe->rect, &spr->calc_rect, spr->angle, &spr->calc_pos, SDL_FLIP_NONE);
				}
			}
			SDL_RenderPresent(sdl_renderer);
		}
	};

};
