#include <raylib.h>
#include <raymath.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <raylib.h>
#include <stdlib.h>

#define TILE_SIZE 48

typedef enum TileKind
{
	TILE_EMPTY,
	TILE_LIT,
	TILE_WALL,
	TILE_LAMP,

	TILE_KIND_COUNT
} TileKind;

typedef struct Tile
{
	TileKind kind;
	int lampRequirement;
} Tile;

typedef struct Level
{
	int tileCountX;
	int tileCountY;
	Tile *tiles;
} Level;

typedef enum ViolationKind
{
	VIOLATION_LAMP_REQUIREMENT,
	VIOLATION_LAMP_LIT_BY_OTHER_LAMP,
} ViolationKind;

typedef struct Violation
{
	ViolationKind kind;
	int tileX;
	int tileY;
} Violation;

typedef struct Violations
{
	Violation *items;
	int count;
	int capacity;
} Violations;

typedef enum Mode
{
	MODE_EDIT,
	MODE_PLAY,
} Mode;

typedef struct Editor
{
	Mode mode;
	Tile tileToDraw;
	Camera2D camera;
	Rectangle view;
	Vector2 previousMousePosition;
	Vector2 previousViewportCenter;
	Vector2 zoomTarget;
	float previousZoom;

	Violations violations;
} Editor;

#define COLOR_WALL (CLITERAL(Color){0x45, 0x56, 0x60, 0xff})
#define COLOR_LAMP (CLITERAL(Color){0xcc, 0xee, 0x30, 0xff})
#define COLOR_LIT (CLITERAL(Color){251, 241, 218, 255})
#define COLOR_BACKGROUND (CLITERAL(Color){0xe0, 0xe0, 0xe5, 0xff})
#define COLOR_BACKGROUND_PLAY (CLITERAL(Color){0xe0, 0xf5, 0xf0, 0xff})
#define COLOR_BACKGROUND_PUZZLE_SOLVED (CLITERAL(Color){0xf5, 0xf5, 0xa3, 0xFF})

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

int Modulo(int n, int m)
{
	return ((n % m) + m) % m;
}

void DrawTileWithAlpha(Tile tile, int tileX, int tileY, float alpha)
{
	Color color;
	switch (tile.kind)
	{
		case TILE_EMPTY: return;
		case TILE_LIT: color = COLOR_LIT; break;
		case TILE_WALL: color = COLOR_WALL; break;
		case TILE_LAMP: color = COLOR_LAMP; break;
		default:
			break;
	}

	float x = tileX * TILE_SIZE;
	float y = tileY * TILE_SIZE;
	DrawRectangle(x, y, TILE_SIZE, TILE_SIZE, ColorAlpha(color, alpha));

	if (tile.kind == TILE_WALL && tile.lampRequirement >= 0)
	{
		char lampCountText[12] = {0};
		snprintf(lampCountText, sizeof(lampCountText), "%d", tile.lampRequirement);
		int fontSize = TILE_SIZE / 2;
		int textLength = MeasureText(lampCountText, fontSize);
		x += (TILE_SIZE/2.0f - textLength/2.0f);
		y += (TILE_SIZE/2.0f - fontSize/2.0f);
		DrawText(lampCountText, x, y, fontSize, WHITE);
	}
}

void DrawTile(Tile tile, int tileX, int tileY)
{
	DrawTileWithAlpha(tile, tileX, tileY, 1.0f);
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
	if (worldPosition.x < 0) --*tileX;
	if (worldPosition.y < 0) --*tileY;
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

void DrawTileOutline(int tileX, int tileY, float thick, Color color)
{
	Rectangle tileRect = {
		tileX,
		tileY,
		TILE_SIZE,
		TILE_SIZE,
	};

	DrawRectangleLinesEx(tileRect, thick, color);
}

void DrawTileCursor(Editor *editor)
{
	int mouseTileX;
	int mouseTileY;
	GetMouseTile(editor->camera, &mouseTileX, &mouseTileY);

	Vector2 tilePosition = {mouseTileX * TILE_SIZE, mouseTileY * TILE_SIZE};

	if (editor->tileToDraw.kind == TILE_EMPTY)
	{
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
	}
	else
	{
		DrawTileWithAlpha(editor->tileToDraw, mouseTileX, mouseTileY, 0.4f);
		DrawTileOutline(tilePosition.x, tilePosition.y, 4, ColorAlpha(WHITE, 0.5f));
	}
}

Tile *GetTile(Level level, int tileX, int tileY)
{
	return &level.tiles[GetTileIndex(level, tileX, tileY)];
}

bool TryGetTile(Level level, int tileX, int tileY, Tile **outTile)
{
	if (!IsTileInLevel(level, tileX, tileY))
	{
		return false;
	}
	
	*outTile = GetTile(level, tileX, tileY);
	return true;
}

void PutTile(Level level, int tileX, int tileY, Tile tile)
{
	if (!IsTileInLevel(level, tileX, tileY))
		return;

	*GetTile(level, tileX, tileY) = tile;
}

void AddViolation(Violations *violations, Violation violation)
{
	if (violations->count + 1 >= violations->capacity)
	{
		violations->capacity *= 2;
		if (violations->capacity == 0)
		{
			violations->capacity = 8;
		}
		violations->items = realloc(violations->items, sizeof(violations->items[0]) * violations->capacity);
		assert(violations->items != NULL);
	}

	violations->items[violations->count] = violation;
	++violations->count;
}

bool GetLampRequirementViolations(Level level, Violations *violations)
{
	bool foundViolation = false;

	for (int tileY = 0; tileY < level.tileCountY; ++tileY)
	{
		for (int tileX = 0; tileX < level.tileCountX; ++tileX)
		{
			Tile tile = level.tiles[GetTileIndex(level, tileX, tileY)];
			if (tile.kind != TILE_WALL)
			{
				continue;
			}

			int lampsNeeded = tile.lampRequirement;

			if (lampsNeeded == -1)
				continue;

			// tile is a wall
			Tile *neighborTile = NULL;

			if (TryGetTile(level, tileX - 1, tileY, &neighborTile) && (neighborTile->kind == TILE_LAMP))
			{
				--lampsNeeded;
			}

			if (TryGetTile(level, tileX + 1, tileY, &neighborTile) && (neighborTile->kind == TILE_LAMP))
			{
				--lampsNeeded;
			}

			if (TryGetTile(level, tileX, tileY - 1, &neighborTile) && (neighborTile->kind == TILE_LAMP))
			{
				--lampsNeeded;
			}

			if (TryGetTile(level, tileX, tileY + 1, &neighborTile) && (neighborTile->kind == TILE_LAMP))
			{
				--lampsNeeded;
			}

			if (lampsNeeded != 0)
			{
				AddViolation(violations, CLITERAL(Violation){
					.kind = VIOLATION_LAMP_REQUIREMENT,
					.tileX = tileX,
					.tileY = tileY,
				});
				foundViolation = true;
			}
		}
	}
	return foundViolation;
}

bool GetLampLitByOtherLampViolations(Level level, Violations *violations)
{
	bool foundViolation = false;

	for (int tileY = 0; tileY < level.tileCountY; ++tileY)
	{
		for (int tileX = 0; tileX < level.tileCountX; ++tileX)
		{
			Tile tile = level.tiles[GetTileIndex(level, tileX, tileY)];
			if (tile.kind != TILE_LAMP)
			{
				continue;
			}

			Tile *checkTile = NULL;

			// Check row to the right
			for (int checkX = tileX + 1; checkX < level.tileCountX; ++checkX)
			{
				checkTile = GetTile(level, checkX, tileY);
				if (checkTile->kind == TILE_LAMP)
				{
					foundViolation = true;
					AddViolation(violations, CLITERAL(Violation){
						.kind = VIOLATION_LAMP_LIT_BY_OTHER_LAMP,
						.tileX = tileX,
						.tileY = tileY,
					});
					AddViolation(violations, CLITERAL(Violation){
						.kind = VIOLATION_LAMP_LIT_BY_OTHER_LAMP,
						.tileX = checkX,
						.tileY = tileY,
					});
					break;
				}

				if (checkTile->kind == TILE_WALL)
				{
					break;
				}
			}

			// Check column
			for (int checkY = tileY + 1; checkY < level.tileCountY; ++checkY)
			{
				checkTile = GetTile(level, tileX, checkY);
				if (checkTile->kind == TILE_LAMP)
				{
					foundViolation = true;
					AddViolation(violations, CLITERAL(Violation){
						.kind = VIOLATION_LAMP_LIT_BY_OTHER_LAMP,
						.tileX = tileX,
						.tileY = tileY,
					});
					AddViolation(violations, CLITERAL(Violation){
						.kind = VIOLATION_LAMP_LIT_BY_OTHER_LAMP,
						.tileX = tileX,
						.tileY = checkY,
					});
					break;
				}

				if (checkTile->kind == TILE_WALL)
				{
					break;
				}
			}
		}
	}
	return foundViolation;
}

bool GetViolations(Level level, Violations *violations)
{
	// For all wall tiles,
	//   if lamp requirement, check neighbor tiles for correct amount of lamps
	bool foundViolation = false;
	foundViolation |= GetLampRequirementViolations(level, violations);
	
	// For all lamp tiles,
	//   Check that they are not lit by another lamp
	foundViolation |= GetLampLitByOtherLampViolations(level, violations);


	// If any ground tiles are not lit,
	//   We have not completed the puzzle
	return foundViolation;
}

Vector2 WorldCoordinateFromTile(int tileX, int tileY)
{
	return (Vector2){
		tileX * TILE_SIZE,
		tileY * TILE_SIZE,
	};
}

void DrawViolations(Violations *violations)
{
	for (int i = 0; i < violations->count; ++i)
	{
		Violation violation = violations->items[i];
		assert(violation.kind == VIOLATION_LAMP_REQUIREMENT || violation.kind == VIOLATION_LAMP_LIT_BY_OTHER_LAMP);

		Vector2 tileCoord = WorldCoordinateFromTile(violation.tileX, violation.tileY);

		DrawTileOutline(tileCoord.x, tileCoord.y, 4, RED);
	}
}

bool IsPuzzleSolved(Level level, Editor *editor)
{
	for (int tileY = 0; tileY < level.tileCountY; ++tileY)
	{
		for (int tileX = 0; tileX < level.tileCountX; ++tileX)
		{
			if (GetTile(level, tileX, tileY)->kind == TILE_EMPTY)
			{
				return false;
			}
		}
	}

	editor->violations.count = 0;
	GetViolations(level, &editor->violations);
	return editor->violations.count == 0;
}

void Draw(Level level, Editor *editor)
{
	Color backgroundColor = COLOR_BACKGROUND;

	if (editor->mode == MODE_PLAY)
	{
		backgroundColor = COLOR_BACKGROUND_PLAY;
		if (IsPuzzleSolved(level, editor))
		{
			backgroundColor = COLOR_BACKGROUND_PUZZLE_SOLVED;
		}
	}


	ClearBackground(backgroundColor);
	BeginMode2D(editor->camera);
	{
		DrawRectangle(0, 0, level.tileCountX*TILE_SIZE, level.tileCountY*TILE_SIZE, WHITE);
		DrawTileGridLines(level.tileCountX, level.tileCountY);
		DrawTileGrid(level);
		DrawTileCursor(editor);
	
		editor->violations.count = 0;
		if (GetViolations(level, &editor->violations))
		{
			DrawViolations(&editor->violations);
		}
	}
	EndMode2D();

	DrawText(TextFormat("Zoom: %d%%", (int)(editor->camera.zoom*100.0f)), 10, 10, 20.0f, BLACK);

#if DRAW_DEBUG_TEXT
	int mouseTileX, mouseTileY;
	GetMouseTile(editor->camera, &mouseTileX, &mouseTileY);
	DrawText(TextFormat("Cursor Position: (%d, %d)", mouseTileX, mouseTileY), 10, 80, 20.0f, BLACK);
#endif
}

void PutTileLine(Level level, int xStart, int yStart, int xEnd, int yEnd, Tile tile)
{
	int xDelta = xEnd - xStart;
	int xStep = 1;
	if (xEnd < xStart) { xStep = -1; xDelta = -xDelta; }
	
	int yDelta = yStart - yEnd;
	int yStep = 1;
	if (yEnd < yStart) { yStep = -1; yDelta = -yDelta; }
	
	int x = xStart;
	int y = yStart;
	int error = xDelta + yDelta;

	for (;;)
	{
		PutTile(level, x, y, tile);
		if (x == xEnd && y == yEnd) return;

		int e2 = 2 * error;

		if (e2 >= yDelta)
		{
			if (x == xEnd) break;
			error += yDelta;
			x += xStep;
		}

		if (e2 <= xDelta)
		{
			if (y == yEnd) break;
			error += xDelta;
			y += yStep;
		}
	}
}

void CameraSetZoomTarget(Camera2D *camera, Vector2 target)
{
	Vector2 worldTarget = GetScreenToWorld2D(target, *camera);
	camera->offset = target;
	camera->target = worldTarget;
}

void CameraZoomByFactor(Camera2D *camera, float zoomFactor, float zoomMin, float zoomMax)
{
	camera->zoom += zoomFactor * camera->zoom;
	camera->zoom = Clamp(camera->zoom, zoomMin, zoomMax);
}

void UpdateLitTiles(Level level)
{
	for (int tileY = 0; tileY < level.tileCountY; ++tileY)
	{
		for (int tileX = 0; tileX < level.tileCountX; ++tileX)
		{
			Tile *tile = GetTile(level, tileX, tileY);
			if (tile->kind == TILE_LIT)
			{
				tile->kind = TILE_EMPTY;
			}
		}
	}

	for (int tileY = 0; tileY < level.tileCountY; ++tileY)
	{
		for (int tileX = 0; tileX < level.tileCountX; ++tileX)
		{
			Tile *tile = GetTile(level, tileX, tileY);
			if (tile->kind != TILE_LAMP)
			{
				continue;
			}
			
			// ray right
			for (int checkX = tileX + 1; checkX < level.tileCountX; ++checkX)
			{
				Tile *checkTile = GetTile(level, checkX, tileY);
				if (checkTile->kind == TILE_EMPTY)
				{
					checkTile->kind = TILE_LIT;
				}
				else if (checkTile->kind == TILE_WALL)
				{
					break;
				}
			}

			// ray left
			for (int checkX = tileX - 1; checkX >= 0; --checkX)
			{
				Tile *checkTile = GetTile(level, checkX, tileY);
				if (checkTile->kind == TILE_EMPTY)
				{
					checkTile->kind = TILE_LIT;
				}
				else if (checkTile->kind == TILE_WALL)
				{
					break;
				}
			}
			
			// ray down
			for (int checkY = tileY + 1; checkY < level.tileCountY; ++checkY)
			{
				Tile *checkTile = GetTile(level, tileX, checkY);
				if (checkTile->kind == TILE_EMPTY)
				{
					checkTile->kind = TILE_LIT;
				}
				else if (checkTile->kind == TILE_WALL)
				{
					break;
				}
			}
			
			// ray up
			for (int checkY = tileY - 1; checkY >= 0; --checkY)
			{
				Tile *checkTile = GetTile(level, tileX, checkY);
				if (checkTile->kind == TILE_EMPTY)
				{
					checkTile->kind = TILE_LIT;
				}
				else if (checkTile->kind == TILE_WALL)
				{
					break;
				}
			}
		}
	}
}

void HandleInput(Level level, Editor *editor)
{
	// Zoom based on mouse wheel
	float wheel = GetMouseWheelMove();
	Vector2 mousePosition = GetMousePosition();
	Vector2 mouseDifference = Vector2Subtract(mousePosition, editor->previousMousePosition);

	if (IsKeyPressed(KEY_TAB))
	{
		if (editor->mode == MODE_EDIT)
		{
			editor->mode = MODE_PLAY;
			editor->tileToDraw = CLITERAL(Tile){TILE_LAMP, .lampRequirement = -1};
		}
		else
		{
			editor->mode = MODE_EDIT;
			editor->tileToDraw = CLITERAL(Tile){TILE_WALL, .lampRequirement = -1};
		}
	}

	if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
	{
		editor->zoomTarget = mousePosition;
	}

	bool isZooming =
		IsKeyDown(KEY_LEFT_CONTROL) &&
		((wheel != 0) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT));

	if (isZooming)
	{
		SetMouseCursor(MOUSE_CURSOR_IBEAM);
		
		Vector2 zoomTarget = mousePosition;
		float zoomFactor = 0.1f * wheel;
		
		if (wheel == 0)
		{
			zoomTarget = editor->zoomTarget;
			zoomFactor = 3.0*mouseDifference.x / GetRenderWidth();

			// editor->previousMousePosition = Vector2Add(editor->previousMousePosition, Vector2Subtract(editor->zoomTarget, mousePosition));
			// SetMousePosition(editor->zoomTarget.x, editor->zoomTarget.y);
		}

		CameraSetZoomTarget(&editor->camera, zoomTarget);
		CameraZoomByFactor(&editor->camera, zoomFactor, 0.05f, 10.0f);
	}
	else if (wheel != 0)
	{
		if (editor->mode == MODE_EDIT)
		{
			editor->tileToDraw.lampRequirement = Modulo(editor->tileToDraw.lampRequirement + 1 + wheel, 6) - 1;
		}
		else
		{
			// Left as an excercise for the reader
		}
	}	

	bool isDragging =
		IsKeyDown(KEY_SPACE) ||
		IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) ||
		(IsKeyDown(KEY_LEFT_CONTROL) && IsMouseButtonDown(MOUSE_BUTTON_LEFT));

	if (isDragging)
	{
		SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL);
		editor->camera.offset = Vector2Add(editor->camera.offset, mouseDifference);
	}
	else if (!isZooming)
	{
		SetMouseCursor(MOUSE_CURSOR_DEFAULT);

		int mouseTileX;
		int mouseTileY;
		GetMouseTile(editor->camera, &mouseTileX, &mouseTileY);

		if (editor->mode == MODE_EDIT)
		{
			// Place tile with left mouse button
			if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
			{

				int prevMouseTileX;
				int prevMouseTileY;
				TileFromVector2(GetScreenToWorld2D(editor->previousMousePosition, editor->camera), &prevMouseTileX, &prevMouseTileY);

				Tile tile = editor->tileToDraw;

				if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
				{
					tile = CLITERAL(Tile){TILE_EMPTY};
				}

				PutTileLine(level, prevMouseTileX, prevMouseTileY, mouseTileX, mouseTileY, tile);
				UpdateLitTiles(level);
			}

			Tile *tile = NULL;
			if (TryGetTile(level, mouseTileX, mouseTileY, &tile))
			{
				if (IsKeyPressed(KEY_ONE))              tile->lampRequirement = 1;
				else if (IsKeyPressed(KEY_TWO))         tile->lampRequirement = 2;
				else if (IsKeyPressed(KEY_THREE))       tile->lampRequirement = 3;
				else if (IsKeyPressed(KEY_FOUR))        tile->lampRequirement = 4;
				else if (IsKeyPressed(KEY_ZERO))        tile->lampRequirement = 0;
				else if (IsKeyPressed(KEY_BACKSPACE))   tile->lampRequirement = -1;
			}
		}
		else
		{
			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
			{
				Tile *tile = NULL;
				if (TryGetTile(level, mouseTileX, mouseTileY, &tile) && tile->kind != TILE_WALL)
				{
					TileKind tileKind = TILE_LAMP;
					if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
					{
						tileKind = TILE_EMPTY;
					}

					tile->kind = tileKind;
					UpdateLitTiles(level);
				}
			}
		}
	}

	editor->previousMousePosition = mousePosition;
}

Vector2 GetViewportCenter()
{
	return CLITERAL(Vector2)
	{
		GetRenderWidth() * 0.5f,
		GetRenderHeight() * 0.5f,
	};
}

void ReadjustViewport(Editor *editor)
{
	if (IsWindowResized())
	{
		Vector2 viewportCenter = GetViewportCenter();
		Vector2 viewportCenterDiff = Vector2Subtract(viewportCenter, editor->previousViewportCenter);
		editor->camera.offset = Vector2Add(editor->camera.offset, viewportCenterDiff);
		editor->previousViewportCenter = viewportCenter;
	}
}

void Update(Level level, Editor *editor)
{
	ReadjustViewport(editor);
	HandleInput(level, editor);
}

float GetLevelWidth(Level level)
{
	return level.tileCountX * TILE_SIZE;
}

float GetLevelHeight(Level level)
{
	return level.tileCountY * TILE_SIZE;
}

Vector2 GetLevelDimensions(Level level)
{
	return CLITERAL(Vector2){
		GetLevelWidth(level),
		GetLevelHeight(level),
	};
}

int main(void)
{
	Level level = {
		.tileCountX = 8,
		.tileCountY = 8,
	};

	Tile tiles[level.tileCountX * level.tileCountY];
	memset(tiles, 0, sizeof(tiles));

	level.tiles = tiles;

	Editor editor = {
		.tileToDraw = {TILE_WALL, .lampRequirement = -1},
		.camera = {
			.zoom = 1.0f,
		},
	};

	InitWindow(1920, 1080, "Zenkari");
	SetWindowState(FLAG_WINDOW_RESIZABLE);

	editor.previousViewportCenter = GetViewportCenter();

	editor.camera.offset = (Vector2){
		0.5f*(GetRenderWidth() - GetLevelWidth(level)),
		0.5f*(GetRenderHeight() - GetLevelHeight(level)),
	};

	while (!WindowShouldClose())
	{
		Update(level, &editor);
		BeginDrawing();
		Draw(level, &editor);
		EndDrawing();
	}
	return 0;
}
