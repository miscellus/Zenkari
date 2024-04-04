#include "raylib/src/raylib.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <raylib.h>
#include <raymath.h>

#define TILE_SIZE 48

typedef enum TileKind
{
	TILE_EMPTY,
	TILE_WALL,
	TILE_LAMP,

	TILE_KIND_COUNT
} TileKind;

typedef struct Tile
{
	TileKind kind;
	int lampCount;
} Tile;

typedef struct Level
{
	int tileCountX;
	int tileCountY;
	Tile *tiles;
} Level;

typedef struct Editor
{
	TileKind tileKind;
	Camera2D camera;
	Rectangle view;
	Vector2 previousMousePosition;
} Editor;

#define COLOR_WALL (CLITERAL(Color){0x45, 0x56, 0x60, 0xff})
#define COLOR_LAMP (CLITERAL(Color){0xcc, 0xee, 0x30, 0xff})
#define COLOR_BACKGROUND (CLITERAL(Color){0xe0, 0xe0, 0xe5, 0xff})

void DrawTileGridLines(int tileCountX, int tileCountY)
{
	int width = tileCountX * TILE_SIZE;
	int height = tileCountY * TILE_SIZE;

	for (int i = 0; i <= tileCountX; ++i)
	{
		float x = i * TILE_SIZE;
		DrawLine(x, 0, x, height, GetColor(0xf0f0f0ff));
	}

	for (int i = 0; i <= tileCountY; ++i)
	{
		float y = i * TILE_SIZE;
		DrawLine(0, y, width, y, GetColor(0xf0f0f0ff));
	}
}

void DrawTile(Tile tile, int tileX, int tileY)
{
	Color color;
	switch (tile.kind)
	{
		case TILE_EMPTY: return;
		case TILE_WALL: color = COLOR_WALL; break;
		case TILE_LAMP: color = COLOR_LAMP; break;
		default:
			break;
	}

	float x = tileX * TILE_SIZE;
	float y = tileY * TILE_SIZE;
	DrawRectangle(x, y, TILE_SIZE, TILE_SIZE, color);

	if (tile.kind == TILE_WALL && tile.lampCount >= 0)
	{
		char lampCountText[12] = {0};
		snprintf(lampCountText, sizeof(lampCountText), "%d", tile.lampCount);
		int textLength = TextLength(lampCountText);
		x += (TILE_SIZE/2.0f - textLength/2.0f);
		DrawText(lampCountText, x, y, TILE_SIZE / 2, WHITE);
	}
}

bool IsTileInLevel(Level level, int tileX, int tileY)
{
	return tileX >= 0 && tileX < level.tileCountX
		&& tileY >= 0 && tileY < level.tileCountY;
}

int GetTileIndex(Level level, int tileX, int tileY)
{
	assert(IsTileInLevel(level, tileX, tileY));
	return tileX + tileY * level.tileCountX;
}

void DrawTileGrid(Level level)
{
	for (int tileY = 0; tileY < level.tileCountY; ++tileY)
	{
		for (int tileX = 0; tileX < level.tileCountX; ++tileX)
		{
			Tile tile = level.tiles[GetTileIndex(level, tileX, tileY)];
			DrawTile(tile, tileX, tileY);
		}
	}
}

void TileFromVector2(Vector2 worldPosition, int *tileX, int *tileY)
{
	*tileX = worldPosition.x / TILE_SIZE;
	*tileY = worldPosition.y / TILE_SIZE;
}

Vector2 GetMouseWorldPosition(Camera2D camera)
{
	return GetScreenToWorld2D(GetMousePosition(), camera);
}

void GetMouseTile(Camera2D camera, int *mouseTileX, int *mouseTileY)
{
	Vector2 worldPosition = GetMouseWorldPosition(camera);
	TileFromVector2(worldPosition, mouseTileX, mouseTileY);
}

void DrawTileCursor(Editor *editor)
{
	int mouseTileX;
	int mouseTileY;
	GetMouseTile(editor->camera, &mouseTileX, &mouseTileY);

	Vector2 tilePosition = {mouseTileX * TILE_SIZE, mouseTileY * TILE_SIZE};
	Vector2 tileDimension = {TILE_SIZE, TILE_SIZE};

	switch (editor->tileKind)
	{
		case TILE_EMPTY:
			DrawLineEx(
				CLITERAL(Vector2){mouseTileX * TILE_SIZE, mouseTileY * TILE_SIZE},
				CLITERAL(Vector2){(mouseTileX + 1) * TILE_SIZE, (mouseTileY + 1) * TILE_SIZE},
				3.0f,
				RED);
			DrawLineEx(
				CLITERAL(Vector2){mouseTileX * TILE_SIZE, (mouseTileY + 1) * TILE_SIZE},
				CLITERAL(Vector2){(mouseTileX + 1) * TILE_SIZE, mouseTileY * TILE_SIZE},
				3.0f,
				RED);
			break;
		case TILE_LAMP:
			DrawRectangleV(tilePosition, tileDimension, ColorAlpha(COLOR_LAMP, 0.4f));
			break;
		case TILE_WALL:
			DrawRectangleV(tilePosition, tileDimension, ColorAlpha(COLOR_WALL, 0.4f));
			break;
		default:
			break;
	}
}

Rectangle GetView(void)
{
	return (Rectangle){
		.x = 0.0f,
		.y = 0.0f,
		.width = GetRenderWidth(),
		.height	= GetRenderHeight(),
	};
}

void Draw(Level level, Editor *editor)
{
	ClearBackground(COLOR_BACKGROUND);
	BeginMode2D(editor->camera);
	{
		DrawRectangle(0, 0, level.tileCountX*TILE_SIZE, level.tileCountY*TILE_SIZE, WHITE);
		DrawTileGridLines(level.tileCountX, level.tileCountY);
		DrawTileGrid(level);
		DrawTileCursor(editor);
	}
	EndMode2D();
}

void UpdateBrush(Editor *editor)
{
	if (IsKeyPressed(KEY_W))
	{
		editor->tileKind = TILE_WALL;
	}
	else if (IsKeyPressed(KEY_L))
	{
		editor->tileKind = TILE_LAMP;
	}
}

void PutTile(Level level, int tileX, int tileY, TileKind tileKind)
{
	if (!IsTileInLevel(level, tileX, tileY))
		return;

	Tile *tile = &level.tiles[GetTileIndex(level, tileX, tileY)];

	if (tile->kind == TILE_WALL && tileKind == TILE_WALL)
	{
		tile->lampCount += 1;
		if (tile->lampCount > 4)
		{
			tile->lampCount = -1;
		}
	}
	else
	{
		tile->lampCount = -1;
	}

	tile->kind = tileKind;
}

void MouseInput(Level level, Editor *editor)
{
	// Zoom based on mouse wheel
	float wheel = GetMouseWheelMove();
	Vector2 mousePosition = GetMousePosition();
	if (wheel != 0)
	{
		Vector2 mouseWorldPos = GetScreenToWorld2D(mousePosition, editor->camera);
		editor->camera.offset = mousePosition;
		editor->camera.target = mouseWorldPos;

		editor->camera.zoom += 0.1f * wheel * editor->camera.zoom;
		editor->camera.zoom = Clamp(editor->camera.zoom, 0.1f, 2.0f);
	}

	// Right-click to drag
	if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
	{
		Vector2 mouseDifference = Vector2Subtract(mousePosition, editor->previousMousePosition);
		editor->camera.offset = Vector2Add(editor->camera.offset, mouseDifference);
	}

	if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
	{
		editor->tileKind = (1 + editor->tileKind) % TILE_KIND_COUNT;
	}

	// Place tile with left mouse button
	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
	{
		int mouseTileX;
		int mouseTileY;
		GetMouseTile(editor->camera, &mouseTileX, &mouseTileY);
		PutTile(level, mouseTileX, mouseTileY, editor->tileKind);
	}

	editor->previousMousePosition = mousePosition;
}

void Update(Level level, Editor *editor)
{
	UpdateBrush(editor);
	MouseInput(level, editor);
}

int main(void)
{
	Level level = {
		.tileCountX = 20,
		.tileCountY = 20,
	};

	Tile tiles[level.tileCountX * level.tileCountY];
	memset(tiles, 0, sizeof(tiles));

	level.tiles = tiles;

	Editor editor = {
		.tileKind = TILE_WALL,
		.camera = {.zoom = 1.0f},
	};

	InitWindow(800, 600, "Zenkari");
	SetWindowState(FLAG_WINDOW_RESIZABLE);

	while (!WindowShouldClose())
	{
		Update(level, &editor);
		BeginDrawing();
		Draw(level, &editor);
		EndDrawing();
	}
	return 0;
}
