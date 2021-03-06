#include "graphics.h"

#include <unordered_set>

Graphics::Graphics(const glm::uvec2 & windowSize, const glm::vec3 & eyePos, const glm::vec3 & eyeFocus, const GameInfo & game) :
		m_camera(windowSize.x, windowSize.y, eyePos, eyeFocus), m_yaw(0.f), m_pitch(0.f), m_board(nullptr), m_generation(0), m_isMultiplayer(false), m_screenSize(
				windowSize) {

	// Used for the linux OS
#if !defined(__APPLE__) && !defined(MACOSX)
	glewExperimental = GL_TRUE;

	const GLenum status = glewInit();

	// This is here to grab the error that comes from glew init.
	// This error is an GL_INVALID_ENUM that has no effects on the performance
	glGetError();

	//Check for error
	if (status != GLEW_OK) {
		std::string errorMsg = "Graphics could not be created! GLEW Error: ";
		errorMsg += ((char*) glewGetErrorString(status));
		throw errorMsg;
	}
#endif

	// For OpenGL 3
	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	//enable depth testing
	glEnable (GL_DEPTH_TEST);
	glDepthFunc (GL_LESS);

	glEnable (GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	m_calculatingGeneration.store(false);
	m_isAutoplaying = false;
	m_autoplayInterval = game.m_autoplayInterval; //quarter second interval
	m_board = new Board(game); //load board

	srand(time(nullptr));

	printf("\nYou are now in SinglePlayer mode!\n");
}

Graphics::~Graphics(void) {
	if (m_calculatingGeneration.load()) {
		m_generationThread->join();
	} else if (m_isAutoplaying.load()) {
		StopAutoplay();
	}
	delete m_board;
}

void Graphics::AddShaderSet(const std::string & setName, const std::string & vertexShaderSrc, const std::string & fragmentShaderSrc) {
	m_board->AddShaderSet(setName, vertexShaderSrc, fragmentShaderSrc);
}

void Graphics::UseShaderSet(const std::string & setName) {
	m_board->UseShaderSet(setName);
	m_board->UpdateLightBindings();
	UpdateCameraBindings();
}

void Graphics::Update(unsigned int dt) {
	m_board->Update();
	m_board->UpdateSpotlightLoc(GetEyePos(), glm::normalize(GetEyeFocus() - GetEyePos()), std::cos(3.14159 * 20.0 / 180));//temporary, replace w/ variable later for changing value
}

void Graphics::Render(void) {
	//Clear the screen
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	m_board->Render();

	//Get any errors from OpenGL
	const GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		std::string errMsg = std::string("Error initializing OpenGL! ") + std::to_string(error) + std::string(" : ") + ErrorString(error);
		throw errMsg;
	}
}

void Graphics::ZoomIn(float moveAmount) {
	glm::vec3 newEyePos = GetEyePos();
	glm::vec3 moveVector = glm::normalize(GetEyeFocus() - newEyePos);
	newEyePos += moveVector;
	UpdateCamera(newEyePos, GetEyeFocus());
}

void Graphics::ZoomOut(float moveAmount) {
	glm::vec3 newEyePos = GetEyePos();
	glm::vec3 moveVector = glm::normalize(GetEyeFocus() - newEyePos);
	newEyePos -= moveVector;
	UpdateCamera(newEyePos, GetEyeFocus());
}

void Graphics::MoveForward(float moveAmount) {
	glm::vec3 newEyePos = GetEyePos();
	glm::vec3 newEyeLoc = GetEyeFocus();
	glm::vec3 moveVector = glm::normalize(GetEyeFocus() - newEyePos);
	newEyePos += moveVector * moveAmount;
	newEyeLoc += moveVector * moveAmount;
	UpdateCamera(newEyePos, newEyeLoc);
}

void Graphics::MoveBackward(float moveAmount) {
	glm::vec3 newEyePos = GetEyePos();
	glm::vec3 newEyeLoc = GetEyeFocus();
	glm::vec3 moveVector = glm::normalize(GetEyeFocus() - newEyePos);
	newEyePos -= moveVector * moveAmount;
	newEyeLoc -= moveVector * moveAmount;
	UpdateCamera(newEyePos, newEyeLoc);
}

void Graphics::MoveRight(float moveAmount) {
	glm::vec3 newEyePos = GetEyePos();
	glm::vec3 newEyeLoc = GetEyeFocus();
	glm::vec3 pointVector = newEyeLoc - newEyePos;
	glm::vec3 upVector = glm::vec3(0, 1, 0);
	glm::vec3 moveVector = glm::normalize(glm::cross(pointVector, upVector));
	newEyePos += moveVector * moveAmount;
	newEyeLoc += moveVector * moveAmount;
	UpdateCamera(newEyePos, newEyeLoc);
}

void Graphics::MoveLeft(float moveAmount) {
	glm::vec3 newEyePos = GetEyePos();
	glm::vec3 newEyeLoc = GetEyeFocus();
	glm::vec3 pointVector = newEyeLoc - newEyePos;
	glm::vec3 upVector = glm::vec3(0, 1, 0);
	glm::vec3 moveVector = glm::normalize(glm::cross(pointVector, upVector));
	newEyePos -= moveVector * moveAmount;
	newEyeLoc -= moveVector * moveAmount;
	UpdateCamera(newEyePos, newEyeLoc);
}

void Graphics::MoveUp(float moveAmount) {
	glm::vec3 newEyePos(GetEyePos());
	glm::vec3 newEyeLoc(GetEyeFocus());
	newEyePos += glm::vec3(0.0, 1.0, 0.0) * moveAmount;
	newEyeLoc += glm::vec3(0.0, 1.0, 0.0) * moveAmount;
	UpdateCamera(newEyePos, newEyeLoc);
}

void Graphics::MoveDown(float moveAmount) {
	glm::vec3 newEyePos(GetEyePos());
	glm::vec3 newEyeLoc(GetEyeFocus());
	newEyePos -= glm::vec3(0.0, 1.0, 0.0) * moveAmount;
	newEyeLoc -= glm::vec3(0.0, 1.0, 0.0) * moveAmount;
	UpdateCamera(newEyePos, newEyeLoc);
}

void Graphics::RotateCamera(float newX, float newY) {
	const float sensitivity = 0.9;
	newX *= sensitivity;
	newY *= sensitivity;

	m_yaw += newX;
	m_pitch += newY;

	if (m_pitch > 89.0)
		m_pitch = 89.0;
	else if (m_pitch < -89.0)
		m_pitch = -89.0;

	glm::vec3 front;
	front.x = std::cos(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
	front.y = -std::sin(glm::radians(m_pitch));
	front.z = std::sin(sin(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch)));
	glm::normalize(front);

	UpdateCamera(GetEyePos(), GetEyePos() + front);
}

void Graphics::UpdateCamera(const glm::vec3 & eyePos, const glm::vec3 & eyeFocus) {
	m_camera.UpdateCamera(eyePos, eyeFocus);
	UpdateCameraBindings();
}

glm::vec3 Graphics::GetEyePos(void) const {
	return m_camera.GetEyePos();
}

glm::vec3 Graphics::GetEyeFocus(void) const {
	return m_camera.GetEyeFocus();
}

void Graphics::ChangeAmbientLight(const glm::vec3 & change) {
	m_board->ChangeAmbientLight(change);
}

void Graphics::ChangeDiffuseLight(const glm::vec3 & change) {
	m_board->ChangeDiffuseLight(change);
}

void Graphics::ChangeSpecularLight(const glm::vec3 & change) {
	m_board->ChangeSpecularLight(change);
}

//call this from engine, and it should successfully click on object

void Graphics::LeftClick(const glm::vec2 & mousePosition) {
	glm::uvec3 elementClicked;
	try {
		elementClicked = m_board->GetGameElementByPosition(GetPositionUnder(mousePosition));
	} catch (const std::string & e) {
#ifdef DEBUG
		printf("\n%s\n", e.c_str());
#endif
		return;
	}

	ObjType type = m_board->GetGameElementType(elementClicked);

	// If it is a multiplayer game
	if (m_isMultiplayer) {
		if (type == DEAD && m_ownCellsKilled == 2 && !m_hasPlacedNewCell && !m_hasMarkedEnemyCell) { // clicking on a dead cell
			//change according to which player
			(m_playerTurnFlag) ?
					m_board->SetGameElementType(elementClicked, P1_ALIVE_MARKED) : m_board->SetGameElementType(elementClicked, P2_ALIVE_MARKED);
			m_hasPlacedNewCell = true;
		} else if (type == P2_ALIVE && m_playerTurnFlag && !m_hasMarkedEnemyCell && m_ownCellsKilled == 0) { // player 1 is marking a player 2 cell for death
			m_board->SetGameElementType(elementClicked, P1_DEAD_MARKED);
			m_hasMarkedEnemyCell = true;
		} else if (type == P1_ALIVE && !m_playerTurnFlag && !m_hasMarkedEnemyCell && m_ownCellsKilled == 0) { // player 2 is marking a player 1 cell for death
			m_board->SetGameElementType(elementClicked, P2_DEAD_MARKED);
			m_hasMarkedEnemyCell = true;
		} else if (type == P1_ALIVE && m_playerTurnFlag && m_ownCellsKilled < 2 && !m_hasMarkedEnemyCell) { //player 1 is marking one of their own cells for death
			m_board->SetGameElementType(elementClicked, P1_DEAD_MARKED);
			++m_ownCellsKilled;
		} else if (type == P2_ALIVE && !m_playerTurnFlag && m_ownCellsKilled < 2 && !m_hasMarkedEnemyCell) { // player 2 is marking one of their own cells for death
			m_board->SetGameElementType(elementClicked, P2_DEAD_MARKED);
			++m_ownCellsKilled;
		}
	} else {

		if (type == DEAD)
			m_board->SetGameElementType(elementClicked, P1_ALIVE_MARKED);
		else if (type == P1_ALIVE_MARKED)
			m_board->SetGameElementType(elementClicked, P2_ALIVE_MARKED);
		else
			m_board->SetGameElementType(elementClicked);
	}
}

struct PositionComparator {
public:

	inline bool operator()(const std::pair<glm::uvec3, ObjType> & x1, const std::pair<glm::uvec3, ObjType> & x2) const {
		return x1.first == x2.first;
	}
};

struct PositionHasher {

	inline std::size_t operator()(const std::pair<glm::uvec3, ObjType> & x) const {
		return (std::hash<unsigned int>()(x.first.x * 100 + x.first.y * 100 + x.first.z * 100));
	}
};

// Updates the board one generation, according to Conway's rules

void Graphics::MoveForwardGeneration(void) {
	m_calculatingGeneration.store(true);
#ifdef DEBUG
	printf("launching thread");
#endif
	m_generationThread = new std::thread([this] {
		glm::uvec3 tempElement(0, 0, 0);
		std::unordered_set<std::pair<glm::uvec3, ObjType>, PositionHasher, PositionComparator> updates; //keep track of updated in hash table - efficient

			do { //go through all the elements
				ObjType tempElementType = m_board->GetGameElementType(tempElement);

				//update marked ones
				if (tempElementType == P1_DEAD_FUTURE || tempElementType == P1_DEAD_MARKED || tempElementType == P2_DEAD_FUTURE
						|| tempElementType == P2_DEAD_MARKED) {
					this->m_board->SetGameElementType(tempElement);
					tempElementType = DEAD;
				} else if (tempElementType == P1_ALIVE_FUTURE || tempElementType == P1_ALIVE_MARKED) {
					this->m_board->SetGameElementType(tempElement, P1_ALIVE);
					tempElementType = P1_ALIVE;
				} else if (tempElementType == P2_ALIVE_FUTURE || tempElementType == P2_ALIVE_MARKED) {
					this->m_board->SetGameElementType(tempElement, P2_ALIVE);
					tempElementType = P2_ALIVE;
				}

				int aliveNeighbors = 0, blueNeighbors = 0, redNeighbors = 0;
				bool isAlive = false;
				std::vector < glm::uvec3 > neighbors = this->m_board->GetGameElementNeighbors(tempElement);

				if (tempElementType == P1_ALIVE || tempElementType == P2_ALIVE)
				isAlive = true;

				//get count of neighbors
				for (const glm::uvec3 & e : neighbors) {
					ObjType typeTemp = m_board->GetGameElementType(e);
					if (typeTemp == P1_ALIVE || typeTemp == P1_ALIVE_FUTURE || typeTemp == P1_ALIVE_MARKED) {
						++aliveNeighbors;
						++blueNeighbors;
					} else if (typeTemp == P2_ALIVE || typeTemp == P2_ALIVE_FUTURE || typeTemp == P2_ALIVE_MARKED) {
						++aliveNeighbors;
						++redNeighbors;
					}
				}

				if (isAlive && (aliveNeighbors < 2 || aliveNeighbors > 3)) { //rule 1 & 3 - death by under & over population
					(tempElementType == P1_ALIVE) ?
					updates.insert(std::pair<glm::uvec3, ObjType>(tempElement, P1_DEAD_FUTURE)) :
					updates.insert(std::pair<glm::uvec3, ObjType>(tempElement, P2_DEAD_FUTURE));
				} else if (!isAlive && aliveNeighbors == 3) { //rule 4 - growth
					(blueNeighbors > redNeighbors) ?
					updates.insert(std::pair<glm::uvec3, ObjType>(tempElement, P1_ALIVE_FUTURE)) :
					updates.insert(std::pair<glm::uvec3, ObjType>(tempElement, P2_ALIVE_FUTURE));
				}
				tempElement = this->m_board->GetNextGameElement(tempElement); //go to next element
			}while (tempElement != glm::uvec3(0, 0, 0)); //check if all have been iterated through

			//update elements
			for (const std::pair<glm::uvec3, ObjType> & x : updates) {
				this->m_board->SetGameElementType(x.first, x.second);
			}

			this->m_generation = this->m_generation + 1;

			printf("\nGeneration %i done!\n", m_generation);
#ifdef DEBUG
			printf("Exiting thread\n");
#endif
			m_calculatingGeneration.store(false);
		});
	m_generationThread->detach();
}

bool Graphics::IsGenerating(void) const {
	return m_calculatingGeneration.load();
}

bool Graphics::IsMultiplayer(void) const {
	return m_isMultiplayer;
}

bool Graphics::IsAutoplaying(void) const {
	return m_isAutoplaying.load();
}

void Graphics::StartAutoplay(void) {
	m_isAutoplaying = true;
	m_autoplayThread = new std::thread([this]() {
		std::chrono::high_resolution_clock::time_point t1, t2;
		float duration;
		while (this->m_isAutoplaying.load()) {
			t1 = std::chrono::high_resolution_clock::now();
			this->MoveForwardGeneration();
			while (m_calculatingGeneration.load());
			t2 = std::chrono::high_resolution_clock::now();
			duration = std::chrono::duration_cast < std::chrono::milliseconds > (t2 - t1).count();
			if (duration < m_autoplayInterval)
			std::this_thread::sleep_for(std::chrono::milliseconds((int) (this->m_autoplayInterval - duration)));
		}
	});
	m_autoplayThread->detach();
}

void Graphics::StopAutoplay(void) {
	m_isAutoplaying.store(false);
	while (m_calculatingGeneration)
		;
}

//Changes between player 1 and player 2

void Graphics::ChangePlayer(void) {
	if (m_isMultiplayer) {
		if (m_playerTurnFlag) { //move to player 2 turn
			m_playerTurnFlag = false;
		} else { //go to next generation and then back to player 1 turn
			MoveForwardGeneration();
			m_playerTurnFlag = true;
		}
		m_hasPlacedNewCell = m_hasMarkedEnemyCell = false;
		m_ownCellsKilled = 0;

		m_playerTurnFlag ? printf("\nIt is now Player 1 (Blue) turn\n") : printf("\nIt is now Player 2 (Red) turn\n");
		printf("mark 2 of your cells for death & 1 dead cell for life OR 1 opponent cell for death\n");
		printf("press 'p' to progress to next players turn\n");

	} else {
		MoveForwardGeneration();
	}
}

void Graphics::Reset(void) {
	printf("\nResetting everything!\n");
	glm::uvec3 tempElement(0, 0, 0);
	do { //go through all the elements
		m_board->SetGameElementType(tempElement);
		tempElement = m_board->GetNextGameElement(tempElement); //go to next element
	} while (tempElement != glm::uvec3(0, 0, 0)); //check if all have been iterated through
	m_generation = 0;
	m_isMultiplayer = false;
	printf("\nYou are now in SinglePlayer mode!\n");
}

// Changes between singleplayer and multiplayer

void Graphics::ChangeGamemode(void) {
	if (m_isMultiplayer) {
		m_isMultiplayer = false;
		printf("\nYou are now in SinglePlayer mode! You can press enter for auto simulation!\n");
	} else { //switch to multiplayer
		glm::uvec3 tempElement(0, 0, 0);
		do { //go through all the elements
			int random = std::rand() % 10; //number between 0-9

			if (random <= 2) //20% chance
				m_board->SetGameElementType(tempElement, P1_ALIVE);
			else if (random <= 4) //20% chance
				m_board->SetGameElementType(tempElement, P2_ALIVE);
			else
				//60% chance - dead
				m_board->SetGameElementType(tempElement);

			tempElement = m_board->GetNextGameElement(tempElement); //go to next element
		} while (tempElement != glm::uvec3(0, 0, 0)); //check if all have been iterated through

		printf("\nYou are now in Multiplayer mode! Blocks have been randomly initailized!\n");
		m_generation = 0;
		MoveForwardGeneration();
		printf("\nIt is now Player 1 (Blue) turn\n");
		printf("mark 2 of your cells for death & 1 dead cell for life OR 1 opponent cell for death\n");
		printf("press 'p' to progress to next players turn\n");
		m_isMultiplayer = true;
	}

	m_hasMarkedEnemyCell = m_hasPlacedNewCell = false;
	m_playerTurnFlag = true;
	m_ownCellsKilled = 0;
}

std::string Graphics::ErrorString(const GLenum error) const {
	if (error == GL_INVALID_ENUM)
		return "GL_INVALID_ENUM: An unacceptable value is specified for an enumerated argument.";
	else if (error == GL_INVALID_VALUE)
		return "GL_INVALID_VALUE: A numeric argument is out of range.";
	else if (error == GL_INVALID_OPERATION)
		return "GL_INVALID_OPERATION: The specified operation is not allowed in the current state.";
	else if (error == GL_INVALID_FRAMEBUFFER_OPERATION)
		return "GL_INVALID_FRAMEBUFFER_OPERATION: The framebuffer object is not complete.";
	else if (error == GL_OUT_OF_MEMORY)
		return "GL_OUT_OF_MEMORY: There is not enough memory left to execute the command.";
	else
		return "None";
}

//updates bindings for camera in shader - need to call for camera change to take effect

void Graphics::UpdateCameraBindings(void) {
	m_board->UpdateCameraBindings(m_camera.GetView(), m_camera.GetProjection(), m_camera.GetEyePos());
}

//raycast and find position mouse is pointing at

glm::vec3 Graphics::GetPositionUnder(const glm::vec2 & mousePosition) {
	float mouseScreenX = (2.0f * mousePosition.x) / m_screenSize.x - 1.0f;
	float mouseScreenY = 1.0f - (2.0f * mousePosition.y) / m_screenSize.y;
	glm::vec4 mouseRayStart(mouseScreenX, mouseScreenY, -1.0f, 1.0f);
	glm::vec4 mouseRayEnd(mouseScreenX, mouseScreenY, 0, 1.0f);

	glm::mat4 inverseProjectionMatrix = glm::inverse(m_camera.GetProjection());
	glm::mat4 inverseViewMatrix = glm::inverse(m_camera.GetView());

	glm::vec4 cameraRayStart = inverseProjectionMatrix * mouseRayStart;
	cameraRayStart /= cameraRayStart.w;
	glm::vec4 cameraRayEnd = inverseProjectionMatrix * mouseRayEnd;
	cameraRayEnd /= cameraRayEnd.w;

	glm::vec4 worldRayStart = inverseViewMatrix * cameraRayStart;
	worldRayStart /= worldRayStart.w;
	glm::vec4 worldRayEnd = inverseViewMatrix * cameraRayEnd;
	worldRayEnd /= worldRayEnd.w;

	glm::vec3 worldRayDirection(worldRayEnd - worldRayStart);
	worldRayDirection = glm::normalize(worldRayDirection);
	glm::vec3 worldRayMax = glm::vec3(worldRayStart.x, worldRayStart.y, worldRayStart.z) + worldRayDirection * 1000.0f;

	btVector3 start, end;
	start = btVector3(worldRayStart.x, worldRayStart.y, worldRayStart.z);
	end = btVector3(worldRayMax.x, worldRayMax.y, worldRayMax.z);

	//get the raycast callback ready
	btCollisionWorld::ClosestRayResultCallback closestResults(start, end);
	closestResults.m_flags |= btTriangleRaycastCallback::kF_FilterBackfaces;

	m_board->GetBulletWorld()->rayTest(start, end, closestResults);

	//if it hit, grab the position of the collider, otherwise throw not found error
	if (closestResults.hasHit()) {
		btVector3 hitResults = closestResults.m_collisionObject->getWorldTransform().getOrigin();
		glm::vec3 cubePosition = glm::vec3(hitResults.x(), hitResults.y(), hitResults.z()); //get position

#ifdef DEBUG
		printf("\nHit found for |%s|:|%s|\n", glm::to_string(mousePosition).c_str(), glm::to_string(cubePosition).c_str());
#endif
		return cubePosition;
	} else {
		throw std::string("No hit found for: " + glm::to_string(mousePosition));
	}
}
