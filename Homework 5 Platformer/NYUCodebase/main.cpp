/*
NAME: Matthew Ricci
CLASS: CS3113 Homework 5 - Platformer
NOTES: 
todo list - make start screen, add collisions, ADD FIXED TIMESTEP!!, add collectibles
*/

#ifdef _WINDOWS
#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include <vector>
#include "Matrix.h"
#include "ShaderProgram.h"
#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
using namespace std;

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

//#define LEVEL_HEIGHT 24
//#define LEVEL_WIDTH 150
//#define SPRITE_COUNT_X 24
//#define SPRITE_COUNT_Y 8
//#define tile_size 16

#define LEVEL_HEIGHT 35
#define LEVEL_WIDTH 198
#define SPRITE_COUNT_X 16
#define SPRITE_COUNT_Y 8
#define TILE_SIZE 0.2
#define GRAVITY 0.0
// 60 FPS (1.0f/60.0f)
#define FIXED_TIMESTEP 0.0166666f
#define MAX_TIMESTEPS 6

SDL_Window* displayWindow;

enum GameState {START_SCREEN, LEVEL_ONE};
int state;

//class SheetSprite for mapping and storing sprites from a sheet
class SheetSprite {
public:
	SheetSprite(unsigned int initTextureID, float initU, float initV, float initWidth, float initHeight, float initSize) :
		textureID(initTextureID), u(initU), v(initV), width(initWidth), height(initHeight), size(initSize){}
	void Draw(ShaderProgram *program){
		glBindTexture(GL_TEXTURE_2D, textureID);
		
		GLfloat texCoords[] = {
			u, v + height,
			u + width, v,
			u, v,
			u + width, v,
			u, v + height,
			u + width, v + height
		};
		float aspect = width / height;
		float vertices[] = {
			-0.5f * size * aspect, -0.5f * size,
			0.5f * size * aspect, 0.5f * size,
			-0.5f * size * aspect, 0.5f * size,
			0.5f * size * aspect, 0.5f * size,
			-0.5f * size * aspect, -0.5f * size,
			0.5f * size * aspect, -0.5f * size };

		glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(program->positionAttribute);

		glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
		glEnableVertexAttribArray(program->texCoordAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	float size;
	unsigned int textureID;
	float u;
	float v;
	float width;
	float height;
};

GLuint LoadTexture(const char *image_path){
	SDL_Surface *testTexture = IMG_Load(image_path);
	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, testTexture->w, testTexture->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, testTexture->pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	SDL_FreeSurface(testTexture);
	return textureID;
}

//class Vector3 {
//public:
//	Vector3() :x(0.0f), y(0.0f), z(0.0f){}
//	Vector3(float initX, float initY, float initZ) : 
//		x(initX), y(initY), z(initZ){}
//
//	float x;
//	float y;
//	float z;
//};

//converts two positions (e.g., player.x and player.y) into spots on the tile grid
void worldToTileCoordinates(float worldX, float worldY, int *gridX, int *gridY) {
	*gridX = (int)(worldX / TILE_SIZE);
	*gridY = (int)(-worldY / TILE_SIZE);
}

//class Entity to give all objects of the game a space to live in the program
class Entity{
public:
	Entity(SheetSprite iSprite, float iWidth, float iHeight, float iX=0.0f, float iY=0.0f) : 
		sprite(iSprite), width(iWidth), height(iHeight), x(iX), y(iY),
	    velocity_x(0.0f), velocity_y(0.0f), acceleration_x(0.0f), acceleration_y(0.0f),
	    isStatic(false), collidedTop(false), collidedBottom(false), collidedLeft(false), collidedRight(false){}
	void Draw(ShaderProgram *program){
		sprite.Draw(program);
	}

	//Vector3 position;
	//Vector3 velocity;
	//Vector3 size;
	//float rotation;
	//SheetSprite sprite;
	//bool alive;

	SheetSprite sprite;
	float x;
	float y;
	float width;
	float height;
	float velocity_x;
	float velocity_y;
	float acceleration_x;
	float acceleration_y;
	bool isStatic;
	bool collidedTop;
	bool collidedBottom;
	bool collidedLeft;
	bool collidedRight;
};

class playerEnt : public Entity{
public:
	//playerEnt(SheetSprite iSprite, Vector3 iPosition, Vector3 iVelocity, Vector3 iSize, int iLives) :
	//	Entity(iSprite, iPosition, iVelocity, iSize), lives(iLives){}		//copy constructor
	playerEnt(SheetSprite iSprite, float iWidth, float iHeight, int iLives, float iX = 0.0f, float iY = 0.0f) :
		Entity(iSprite, iWidth, iHeight, iX, iY), lives(iLives){}		//copy constructor

	void Update(float timeElapsed){
		velocity_y -= GRAVITY * timeElapsed;
		y += velocity_y * timeElapsed;
		worldToTileCoordinates(x, y, &gridX, &gridY);

	}

	int lives;
	int gridX, gridY;
	
};

class bulletEntity : public Entity{		//bulletEntity derived class from Entity
public:
	bulletEntity(SheetSprite iSprite, float iWidth, float iHeight, float iX = 0.0f, float iY = 0.0f) :
		Entity(iSprite, iWidth, iHeight, iX = 0.0f, iY = 0.0f){}		//copy constructor
	float timeAlive = 0.0f;
};

//loads textures for building

//draws a string to screen given a font sheet and a string
void drawText(ShaderProgram *program, int fontTexture, std::string text, float size, float spacing, Matrix *modelMatrix) {
	float texture_size = 1.0 / 16.0f;
	std::vector<float> vertexData;
	std::vector<float> texCoordData;

	for (int i = 0; i < text.size(); i++) {
		float texture_x = (float)(((int)text[i]) % 16) / 16.0f;
		float texture_y = (float)(((int)text[i]) / 16) / 16.0f;
		vertexData.insert(vertexData.end(), {
			((size + spacing) * i) + (-0.5f * size), 0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (0.5f * size), -0.5f * size,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size,
		});
		texCoordData.insert(texCoordData.end(), {
			texture_x, texture_y,
			texture_x, texture_y + texture_size,
			texture_x + texture_size, texture_y,
			texture_x + texture_size, texture_y + texture_size,
			texture_x + texture_size, texture_y,
			texture_x, texture_y + texture_size,
		});
	}
	glUseProgram(program->programID);

	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
	glEnableVertexAttribArray(program->positionAttribute);
	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
	glEnableVertexAttribArray(program->texCoordAttribute);
	glBindTexture(GL_TEXTURE_2D, fontTexture);
	program->setModelMatrix(*modelMatrix);
	glDrawArrays(GL_TRIANGLES, 0, text.size() * 6);

	glDisableVertexAttribArray(program->positionAttribute);
	glDisableVertexAttribArray(program->texCoordAttribute);
}

//detects collision via the basic box-to-box algorithm, return true if detected, false otherwise
bool detectCollision(const Entity *a, const Entity *b){
			//if a's bottom is higher than b's top, no collision
	if ((a->y - (a->height / 2)) > (b->y + (b->height / 2)))
		return false;
			//if a's top if lower than b's bottom, no collision
	else if ((a->y + (a->height / 2)) > (b->y - (b->height / 2)))
		return false;
			//if a's left is larger than b's right, no collision
	else if ((a->x - (a->width / 2)) > (b->x + (b->width / 2)))
		return false;
			//if a's right is smaller than b's left, no collision
	else if ((a->x + (a->width / 2)) < (b->x - (b->width / 2)))
		return false;
	else
		return true;

	//if ((a->position.y - (a->size.y / 2)) > (b->position.y + (b->size.y / 2)))
	//	return false;
	////if a's top if lower than b's bottom, no collision
	//else if ((a->position.y + (a->size.y / 2)) > (b->position.y - (b->size.y / 2)))
	//	return false;
	////if a's left is larger than b's right, no collision
	//else if ((a->position.x - (a->size.x / 2)) > (b->position.x + (b->size.x / 2)))
	//	return false;
	////if a's right is smaller than b's left, no collision
	//else if ((a->position.x + (a->size.x / 2)) < (b->position.x - (b->size.x / 2)))
	//	return false;
	//else
	//	return true;
}

//void genWorldFromTile(ShaderProgram *program, GLuint tilesheet, Matrix *modelMatrix, unsigned char **levelData){
//	float tile_size = 16;
//	vector<float> vertexData;
//	vector<float> texCoordData;
//	for (int y = 0; y < LEVEL_HEIGHT; y++){
//		for (int x = 0; x < LEVEL_WIDTH; x++){
//			float u = (float)(((int)levelData[y][x]) % SPRITE_COUNT_X) / (float)SPRITE_COUNT_X;
//			float v = (float)(((int)levelData[y][x]) / SPRITE_COUNT_X) / (float)SPRITE_COUNT_Y;
//
//			float spriteWidth = 1.0f / (float)SPRITE_COUNT_X;
//			float spriteHeight = 1.0f / (float)SPRITE_COUNT_Y;
//
//			vertexData.insert(vertexData.end(), {
//
//				tile_size * x, -tile_size * y,
//				tile_size * x, (-tile_size * y) - tile_size,
//				(tile_size * x) + tile_size, (-tile_size * y) - tile_size,
//
//				tile_size * x, -tile_size * y,
//				(tile_size * x) + tile_size, (-tile_size * y) - tile_size,
//				(tile_size * x) + tile_size, -tile_size * y
//			});
//
//			texCoordData.insert(texCoordData.end(), {
//				u, v,
//				u, v + (spriteHeight),
//				u + spriteWidth, v + (spriteHeight),
//
//				u, v,
//				u + spriteWidth, v + (spriteHeight),
//				u + spriteWidth, v
//			});
//		}
//	}
//	glBindTexture(GL_TEXTURE_2D, tilesheet);
//	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, &vertexData);
//	glEnableVertexAttribArray(program->positionAttribute);
//
//	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, &texCoordData);
//	glEnableVertexAttribArray(program->texCoordAttribute);
//
//	glDrawArrays(GL_TRIANGLES, 0, 600);
//
//}

//bool readLayerData(std::ifstream &stream, unsigned char **levelData) {
//	string line;
//	while (getline(stream, line)) {
//		if (line == "") { break; }
//		istringstream sStream(line);
//		string key, value;
//		getline(sStream, key, '=');
//		getline(sStream, value);
//		if (key == "data") {
//			for (int y = 0; y < LEVEL_HEIGHT; y++) {
//				getline(stream, line);
//				istringstream lineStream(line);
//				string tile;
//				for (int x = 0; x < LEVEL_WIDTH; x++) {
//					getline(lineStream, tile, ',');
//					unsigned char val = (unsigned char)atoi(tile.c_str());
//					if (val > 0) {
//						// be careful, the tiles in this format are indexed from 1 not 0
//						levelData[y][x] = val - 1;
//					}
//					else {
//						levelData[y][x] = 0;
//					}
//				}
//			}
//		}
//	}
//	return true;
//}



int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640*1.5, 360*1.5, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
#ifdef _WINDOWS
	glewInit();
#endif

	SDL_Event event;
	bool done = false;

	//setting default color
	glClearColor(0.0f, 0.525f, 0.988f, 1.0f);

	//texture coordinates
	glViewport(0, 0, 640*1.5, 360*1.5);
	ShaderProgram program(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	glUseProgram(program.programID);
	GLuint tilesheet = LoadTexture("tilesheet.png");
	int font = LoadTexture("font.png");

	//for keeping viewMatrix constant and for following player movement  
	float vm_x = -3.55f;
	float vm_y = 2.3f;

	GLuint playerTexture = LoadTexture("player.png");
	SheetSprite playerSprite(playerTexture, 0.0f, 0.0f, 40.0f / 40.0f, 40.0f / 40.0f, 0.8f);
	//playerEnt player(playerSprite, Vector3(0.5f, -3.42f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), 1);
	playerEnt player(playerSprite, (playerSprite.size*(playerSprite.width/playerSprite.height)), playerSprite.size, 1, 0.5f, -3.0f);
	Matrix projectionMatrix;
	Matrix modelMatrix;
	Matrix playerModelMatrix;
	Matrix viewMatrix;

	projectionMatrix.setOrthoProjection(-3.55, 3.55, -2.0f, 2.0f, -1.0f, 1.0f);

	//for keeping time
	float lastFrameTicks = 0.0f;




	unsigned char levelData[LEVEL_HEIGHT][LEVEL_WIDTH];
	ifstream stream("only-arne-assets.txt");

	//genWorldFromTile(&program, tilesheet, &modelMatrix, &levelData);
//************************************************************************************************************************

	//readLayerData(stream, *levelData);



	string line;
	for (size_t i = 0; i < 10; i++)     //the following 2 lines skip the header, since it is constant
		getline(stream, line);          //i felt it wasn't necessary to parse the header
	while (getline(stream, line)) {
		if (line == "") { break; }
		istringstream sStream(line);
		string key, value;
		getline(sStream, key, '=');
		getline(sStream, value);
		if (key == "data") {
			for (int y = 0; y < LEVEL_HEIGHT; y++) {
				getline(stream, line);
				istringstream lineStream(line);
				string tile;
				for (int x = 0; x < LEVEL_WIDTH; x++) {
					getline(lineStream, tile, ',');
					unsigned char val = (unsigned char)atoi(tile.c_str());
					if (val > 0) {
						// be careful, the tiles in this format are indexed from 1 not 0
						levelData[y][x] = val - 1;
					}
					else {
						levelData[y][x] = 0;
					}
				}
			}
		}
	}


	float tile_size = 0.2;
	vector<float> vertexData;
	vector<float> texCoordData;
	for (int y = 0; y < LEVEL_HEIGHT; y++){
		for (int x = 0; x < LEVEL_WIDTH; x++){

			if (levelData[y][x] > 0){
				float u = (float)(((int)levelData[y][x]) % SPRITE_COUNT_X) / (float)SPRITE_COUNT_X;
				float v = (float)(((int)levelData[y][x]) / SPRITE_COUNT_X) / (float)SPRITE_COUNT_Y;

				float spriteWidth = 1.0f / (float)SPRITE_COUNT_X;
				float spriteHeight = 1.0f / (float)SPRITE_COUNT_Y;

				vertexData.insert(vertexData.end(), {

					tile_size * x, -tile_size * y,
					tile_size * x, (-tile_size * y) - tile_size,
					(tile_size * x) + tile_size, (-tile_size * y) - tile_size,

					tile_size * x, -tile_size * y,
					(tile_size * x) + tile_size, (-tile_size * y) - tile_size,
					(tile_size * x) + tile_size, -tile_size * y
				});

				texCoordData.insert(texCoordData.end(), {
					u, v,
					u, v + (spriteHeight),
					u + spriteWidth, v + (spriteHeight),

					u, v,
					u + spriteWidth, v + (spriteHeight),
					u + spriteWidth, v
				});
			}
		}
	}


//***********************************************************************************

	while (!done) {
		//for general time-keeping between frames
		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;


		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
			else if (event.type == SDL_KEYDOWN){
				if (state == LEVEL_ONE && event.key.keysym.scancode == SDL_SCANCODE_LEFT){    //press space to shoot
				}
				else if (state == START_SCREEN && event.key.keysym.scancode == SDL_SCANCODE_SPACE)
					state = LEVEL_ONE;
			}
		}

		////for general time-keeping between frames
		//float ticks = (float)SDL_GetTicks() / 1000.0f;
		//std::cout << SDL_GetTicks() << std::endl;
		//float elapsed = ticks - lastFrameTicks;
		//lastFrameTicks = ticks;
		//timeCount += elapsed;

		const Uint8 *keys = SDL_GetKeyboardState(NULL);

		if (state == LEVEL_ONE && keys[SDL_SCANCODE_LEFT]){
			
		}
		if (state == LEVEL_ONE && keys[SDL_SCANCODE_RIGHT]){
			
		}

		glClear(GL_COLOR_BUFFER_BIT);


		float vertices[] = { -0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, -0.5, 0.5, };



		//setting up matrixes
		program.setModelMatrix(modelMatrix);
		program.setProjectionMatrix(projectionMatrix);
		program.setViewMatrix(viewMatrix);


		modelMatrix.identity();


		//*****************************

		glBindTexture(GL_TEXTURE_2D, tilesheet);
		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
		glEnableVertexAttribArray(program.positionAttribute);

		glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
		glEnableVertexAttribArray(program.texCoordAttribute);

		glDrawArrays(GL_TRIANGLES, 0, vertexData.size() / 2);
		viewMatrix.identity();
		viewMatrix.Translate(vm_x, vm_y, 0.0f);
		program.setViewMatrix(viewMatrix);

		//*****************************

		float fixedElapsed = elapsed;
		if (fixedElapsed > FIXED_TIMESTEP * MAX_TIMESTEPS) {
			fixedElapsed = FIXED_TIMESTEP * MAX_TIMESTEPS;
		}
		while (fixedElapsed >= FIXED_TIMESTEP) {
			fixedElapsed -= FIXED_TIMESTEP;
			player.Update(FIXED_TIMESTEP);
		}
		player.Update(fixedElapsed);
		playerModelMatrix.identity();
		playerModelMatrix.Translate(player.x, player.y, 0.0f);
		program.setModelMatrix(playerModelMatrix);
		player.Draw(&program);

		//drawing the start screen text, if gamestate is set to it
		if (state == START_SCREEN){

		}

		//drawing the game level, if the gamestate is set to it
		else {
			//draw the score in the bottom-left corner
			modelMatrix.identity();
		
				
		}


		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		glDisableVertexAttribArray(program.positionAttribute);
		glDisableVertexAttribArray(program.texCoordAttribute);

		SDL_GL_SwapWindow(displayWindow);

	}
	SDL_Quit();
	return 0;
}