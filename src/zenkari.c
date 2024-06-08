#include <raylib.h>
#include <raymath.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <raylib.h>
#include <stdlib.h>

#define TILE_SIZE 64

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
	Level level;
	Tile tileToDraw;
	Camera2D camera;
	Rectangle view;
	Vector2 previousMousePosition;
	Vector2 previousViewportCenter;
	Vector2 zoomTarget;
	float previousZoom;

	Violations violations;

	Font font;

	bool showDebugText;
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

void DrawTileWithAlpha(Tile tile, int tileX, int tileY, Font font, float alpha)
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

	Vector2 tileCorner = {
		tileX * TILE_SIZE,
		tileY * TILE_SIZE,
	};
	DrawRectangle(tileCorner.x, tileCorner.y, TILE_SIZE, TILE_SIZE, ColorAlpha(color, alpha));

	if (tile.kind == TILE_WALL && tile.lampRequirement >= 0)
	{
		Vector2 tileDim = {TILE_SIZE, TILE_SIZE};
		Vector2 tileCenter = Vector2Add(tileCorner, Vector2Scale(tileDim, 0.5f));
		char lampCountText[12] = {0};
		snprintf(lampCountText, sizeof(lampCountText), "%d", tile.lampRequirement);
		float fontSize = TILE_SIZE * 0.61803398875f;
		Vector2 textDimensions = MeasureTextEx(font, lampCountText, fontSize, 0);
		Vector2 textCorner = Vector2Subtract(tileCenter, Vector2Scale(textDimensions, 0.5f));
		DrawTextEx(font, lampCountText, textCorner, fontSize, 0, WHITE);
	}
}

void DrawTile(Tile tile, int tileX, int tileY, Font font)
{
	DrawTileWithAlpha(tile, tileX, tileY, font, 1.0f);
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

void DrawTileGrid(Level level, Font font)
{
	for (int tileY = 0; tileY < level.tileCountY; ++tileY)
	{
		for (int tileX = 0; tileX < level.tileCountX; ++tileX)
		{
			Tile tile = level.tiles[GetTileIndex(level, tileX, tileY)];
			DrawTile(tile, tileX, tileY, font);
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
		DrawTileWithAlpha(editor->tileToDraw, mouseTileX, mouseTileY, editor->font, 0.4f);
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
				if (violations == NULL)
				{
					return true;
				}

				foundViolation = true;
				AddViolation(violations, CLITERAL(Violation){
					.kind = VIOLATION_LAMP_REQUIREMENT,
					.tileX = tileX,
					.tileY = tileY,
				});
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
					if (violations == NULL)
					{
						return true;
					}

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
					if (violations == NULL)
					{
						return true;
					}

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
	bool foundViolation = false;

	// For all wall tiles,
	//   if lamp requirement, check neighbor tiles for correct amount of lamps
	foundViolation |= GetLampRequirementViolations(level, violations);
	
	// For all lamp tiles,
	//   Check that they are not lit by another lamp
	foundViolation |= GetLampLitByOtherLampViolations(level, violations);

	// If any ground tiles are not lit,
	//   We have not completed the puzzle
	return foundViolation;
}

bool HasViolations(Level level)
{
	return GetViolations(level, NULL);
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

bool IsPuzzleSolved(Level level)
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

	return !HasViolations(level);
}

void DrawDebugInfo(Editor *editor)
{
	Rectangle debugTextBounds = {
		0.0f,
		0.0f,
		400.0f,
		GetRenderHeight(),
	};
	DrawRectangleRec(debugTextBounds, (Color){255,255,255,160});
	float fontSize = 24.0f;
	float fontSpacing = 1.0f;
	float pad = 20.0f;
	Vector2 textPos = {pad, pad};
	const char *text = TextFormat("Zoom: %d%%", (int)(editor->camera.zoom*100.0f));
	Vector2 textDimensions = MeasureTextEx(editor->font, text, fontSize, fontSpacing);
	DrawTextEx(editor->font, text, textPos, fontSize, fontSpacing, BLACK);
	textPos.y += textDimensions.y * 1.618034f;
	int mouseTileX, mouseTileY;
	GetMouseTile(editor->camera, &mouseTileX, &mouseTileY);
	text = TextFormat("Cursor Position: (%d, %d)", mouseTileX, mouseTileY);
	DrawTextEx(editor->font, text, textPos, fontSize, fontSpacing, BLACK);
}

void Draw(Editor *editor)
{
	Level level = editor->level;

	Color backgroundColor = COLOR_BACKGROUND;

	if (editor->mode == MODE_PLAY)
	{
		backgroundColor = COLOR_BACKGROUND_PLAY;
		if (IsPuzzleSolved(editor->level))
		{
			backgroundColor = COLOR_BACKGROUND_PUZZLE_SOLVED;
		}
	}

	ClearBackground(backgroundColor);
	BeginMode2D(editor->camera);
	{
		DrawRectangle(0, 0, level.tileCountX*TILE_SIZE, level.tileCountY*TILE_SIZE, WHITE);
		DrawTileGridLines(level.tileCountX, level.tileCountY);
		DrawTileGrid(level, editor->font);
		DrawTileCursor(editor);
	
		editor->violations.count = 0;
		if (GetViolations(level, &editor->violations))
		{
			DrawViolations(&editor->violations);
		}
	}
	EndMode2D();

	if (editor->showDebugText)
	{
		DrawDebugInfo(editor);
	}
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

size_t GetSafeLevelStringSize(Level level)
{
	const size_t size_for_width = 12;
	const size_t size_for_height = 12;

	return sizeof(char) * (
		size_for_width + 1 +
		size_for_height + 1 +
		level.tileCountY * (level.tileCountX + 1) + 1);
}

size_t SaveLevelToString(Level level, char *outputBuffer, size_t outputBufferSize)
{
	char *at = outputBuffer;
	char *end = outputBuffer + outputBufferSize;

	at += snprintf(at, (end - at), "%d\n%d\n", level.tileCountX, level.tileCountY);

	for (int tileY = 0; tileY < level.tileCountY; ++tileY)
	{
		for (int tileX = 0; tileX < level.tileCountX; ++tileX)
		{
			char c = '?';
			Tile tile = *GetTile(level, tileX, tileY);
			switch (tile.kind)
			{
				case TILE_WALL:
				{
					if (tile.lampRequirement >= 0)
					{
						c = tile.lampRequirement | '0';
					}
					else
					{
						c = '#';
					}
				} break;

				case TILE_LAMP: c = 'L'; break;

				case TILE_EMPTY:
				case TILE_LIT: c = '.'; break;
				default:
					break;
			}

			at += snprintf(at, (end - at), "%c", c);
		}
		at += snprintf(at, (end - at), "\n");
	}
	at += snprintf(at, (end - at), "%c", '\0');

	return (size_t)(at - outputBuffer);
}

void EatWhitespace(char **at, char *end)
{
	while (*at < end && **at != '\0' && **at <= ' ') ++*at;
}

bool TryLoadLevelFromString(const char *buffer, size_t bufferSize, Level *outLevel)
{
	Level level = {0};

	char *at = (char *)buffer;
	char *end = at + bufferSize;

	char *next_at;
	level.tileCountX = strtol(at, &next_at, 10);
	if (at == next_at) return false;
	if (level.tileCountX > 2048) return false;
	at = next_at;
	EatWhitespace(&at, end);
	level.tileCountY = strtol(at, &next_at, 10);
	if (at == next_at) return false;
	if (level.tileCountY > 2048) return false;
	at = next_at;

	size_t allocAmount = sizeof(*level.tiles) * level.tileCountX * level.tileCountX;
	level.tiles = (Tile *)realloc(outLevel->tiles, allocAmount);
	assert(level.tiles != NULL);
	memset(level.tiles, 0, allocAmount);

	for (int tileY = 0; tileY < level.tileCountY; ++tileY)
	{
		for (int tileX = 0; tileX < level.tileCountX; ++tileX)
		{
			EatWhitespace(&at, end);
			if (at >= end) goto ErrorReturn;

			char c = *at;
			Tile *tile = GetTile(level, tileX, tileY);
			switch (c)
			{
				case '#':
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				{
					tile->kind = TILE_WALL;
					tile->lampRequirement = (c == '#') ? -1 : c - '0';
				} break;

				case 'L': tile->kind = TILE_LAMP; break;

				case '.': tile->kind = TILE_EMPTY; break;

				default:
					goto ErrorReturn;
			}

			++at;
		}
		EatWhitespace(&at, end);
	}

	*outLevel = level;
	return true;

ErrorReturn:
	return false;
}


float GetLevelWidth(Level level)
{
	return level.tileCountX * TILE_SIZE;
}

float GetLevelHeight(Level level)
{
	return level.tileCountY * TILE_SIZE;
}

Vector2 GetViewportCenter()
{
	return CLITERAL(Vector2)
	{
		GetRenderWidth() * 0.5f,
		GetRenderHeight() * 0.5f,
	};
}

void CenterView(Camera2D *camera, Level level)
{
	float levelWidth = GetLevelWidth(level);
	float levelHeight = GetLevelHeight(level);

	float widthRatio = GetRenderWidth() / levelWidth;
	float heightRatio = GetRenderHeight() / levelHeight;

	camera->zoom = widthRatio < heightRatio ? widthRatio : heightRatio;
	camera->zoom *= 0.90;

	camera->target = CLITERAL(Vector2)
	{
		0.5f*levelWidth,
		0.5f*levelHeight,
	};

	camera->offset = GetViewportCenter();

}

void HandleInput(Editor *editor)
{
	// Zoom based on mouse wheel
	float wheel = GetMouseWheelMove();
	Vector2 mousePosition = GetMousePosition();
	Vector2 mouseDifference = Vector2Subtract(mousePosition, editor->previousMousePosition);

	if (IsKeyPressed(KEY_F11) || (IsKeyDown(KEY_LEFT_ALT) && IsKeyPressed(KEY_ENTER)))
	{
		int monitor = GetCurrentMonitor();
        SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
		ToggleFullscreen();
	}

	if (IsKeyPressed(KEY_F1))
	{
		editor->showDebugText ^= 1;
	}

	if (IsKeyPressed(KEY_F))
	{
		CenterView(&editor->camera, editor->level);
	}

	if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))
	{
		if (IsKeyPressed(KEY_C))
		{
			size_t bufferSize = GetSafeLevelStringSize(editor->level);
			char *buffer = malloc(bufferSize);
			SaveLevelToString(editor->level, buffer, bufferSize);
			SetClipboardText(buffer);
			free(buffer);
		}

		if (IsKeyPressed(KEY_V))
		{
			const char *levelString = GetClipboardText();
			int levelStringLength = strlen(levelString);

			if (TryLoadLevelFromString(levelString, levelStringLength, &editor->level))
			{
				UpdateLitTiles(editor->level);
			}
		}
	}

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

				PutTileLine(editor->level, prevMouseTileX, prevMouseTileY, mouseTileX, mouseTileY, tile);
				UpdateLitTiles(editor->level);
			}

			Tile *tile = NULL;
			if (TryGetTile(editor->level, mouseTileX, mouseTileY, &tile))
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
				if (TryGetTile(editor->level, mouseTileX, mouseTileY, &tile) && tile->kind != TILE_WALL)
				{
					TileKind tileKind = TILE_LAMP;
					if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
					{
						tileKind = TILE_EMPTY;
					}

					tile->kind = tileKind;
					UpdateLitTiles(editor->level);
				}
			}
		}
	}

	editor->previousMousePosition = mousePosition;
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

void Update(Editor *editor)
{
	ReadjustViewport(editor);
	HandleInput(editor);
}

void Init(Editor *editor)
{
	InitWindow(1920, 1080, "Zenkari");
	SetWindowState(FLAG_WINDOW_RESIZABLE);
	SetTargetFPS(30);

	*editor = CLITERAL(Editor){
		.level = {
			.tileCountX = 10,
			.tileCountY = 10,
		},
		.tileToDraw = {TILE_WALL, .lampRequirement = -1},
		.camera = {
			.zoom = 1.0f,
		},
	};

	Level *level = &editor->level;
	level->tiles = (Tile *)calloc(level->tileCountX * level->tileCountY, sizeof(*level->tiles));


	editor->previousViewportCenter = GetViewportCenter();

	CenterView(&editor->camera, editor->level);

	const char *fontPath = "assets/oswald.ttf";
	editor->font = LoadFontEx(fontPath, 256, NULL, 0);
	GenTextureMipmaps(&editor->font.texture);
	SetTextureFilter(editor->font.texture, TEXTURE_FILTER_TRILINEAR);
	SetTextureWrap(editor->font.texture, TEXTURE_WRAP_CLAMP);
}

int main(void)
{
	Editor editor;
	Init(&editor);

	while (!WindowShouldClose())
	{
		Update(&editor);
		BeginDrawing();
		Draw(&editor);
		EndDrawing();
	}
	return 0;
}
