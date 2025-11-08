import pygame
import random


class Particle:
    def __init__(self, x, y, color=(255, 255, 255)):
        self.x = x
        self.y = y
        self.radius = 3
        self.color = color
        self.last_pos = (x, y)
        self.accel = (0, 0);
        self.mass = 1;
    
    def setVelocity(self, vx, vy, dt=1):
        self.last_pos = (self.x - vx * dt, self.y - vy * dt);
        
    def addVelocity(self, vx, vy, dt=1):
        self.last_pos = (self.last_pos[0] - vx * dt, self.last_pos[1] - vy * dt);
        
    def getVelocity(self):
        vx = self.x - self.last_pos[0]
        vy = self.y - self.last_pos[1]
        return (vx, vy)
    
    def accelerate(self, ax, ay):
        # accumulate acceleration components (avoid tuple concatenation)
        self.accel = (self.accel[0] + ax, self.accel[1] + ay)
        
    def update(self, dt=1):
        last_up_move = (self.x - self.last_pos[0], self.y - self.last_pos[1])
        
        # Much lighter dampening - only apply a small drag force
        VELOCITY_DAMPENING = 0.5
        
        new_pos = (
            self.x + last_up_move[0] + (self.accel[0] - last_up_move[0] * VELOCITY_DAMPENING) * (dt * dt),
            self.y + last_up_move[1] + (self.accel[1] - last_up_move[1] * VELOCITY_DAMPENING) * (dt * dt)
        )
        
        self.last_pos = (self.x, self.y)
        self.x = new_pos[0]
        self.y = new_pos[1]
        self.accel = (0, 0)
        
    def stop(self):
        self.last_pos = (self.x, self.y)
        
    def slow_down(self, factor):
        vx = self.x - self.last_pos[0]
        vy = self.y - self.last_pos[1]
        
        vx *= factor
        vy *= factor
        
        self.last_pos += (self.x - vx, self.y - vy)
        
    def set_position(self, x, y):
        self.x = x
        self.y = y
        self.last_pos = (x, y)
        
class Input:
    def __init__(self):
        mouse_pos = pygame.mouse.get_pos()
        self.mouse_x = mouse_pos[0]
        self.mouse_y = mouse_pos[1]
        self.mouse_buttons = pygame.mouse.get_pressed()
        self.keys = pygame.key.get_pressed()
        
    def is_mouse_button_down(self, button_index):
        return self.mouse_buttons[button_index]
    
    def get_mouse_position(self):
        return (self.mouse_x, self.mouse_y)
    
    def is_key_down(self, key):
        return self.keys[key]
    
class QuadTree:
    def __init__(self, boundary, capacity):
        self.boundary = boundary
        self.particles = []
        self.capacity = capacity
        self.divided = False
        
    def insert(self, particle):
        if not self.boundary.collidepoint(particle.x, particle.y):
            return False

        if not self.divided and len(self.particles) < self.capacity:
            self.particles.append(particle)
            return True

        if not self.divided:
            self.subdivide()
            # After subdividing, move existing particles into the appropriate children
            # (do not include the new particle yet)
            existing = self.particles
            self.particles = []
            for p in existing:
                inserted = (self.northeast.insert(p) or
                            self.northwest.insert(p) or
                            self.southeast.insert(p) or
                            self.southwest.insert(p))
                # If for some reason a particle isn't inserted into a child (shouldn't happen), keep it here
                if not inserted:
                    self.particles.append(p)

        return (self.northeast.insert(particle) or
                self.northwest.insert(particle) or
                self.southeast.insert(particle) or
                self.southwest.insert(particle))

    def subdivide(self):
        x = int(self.boundary.x)
        y = int(self.boundary.y)
        w = int(self.boundary.width)
        h = int(self.boundary.height)
        hw = w // 2
        hh = h // 2

        self.northeast = QuadTree(pygame.Rect(x + hw, y, hw, hh), self.capacity)
        self.northwest = QuadTree(pygame.Rect(x, y, hw, hh), self.capacity)
        self.southeast = QuadTree(pygame.Rect(x + hw, y + hh, hw, hh), self.capacity)
        self.southwest = QuadTree(pygame.Rect(x, y + hh, hw, hh), self.capacity)
        self.divided = True
        
    def query(self, range, found):
        if not self.boundary.colliderect(range):
            return

        for p in self.particles:
            if range.collidepoint(p.x, p.y):
                found.append(p)

        if self.divided:
            self.northeast.query(range, found)
            self.northwest.query(range, found)
            self.southeast.query(range, found)
            self.southwest.query(range, found)
            
        return found
    
    def update(self, particle):
        if self.boundary.collidepoint(particle.x, particle.y):
            return
        else:
            if particle in self.particles:
                self.particles.remove(particle)
            return self.insert(particle)
        

    def visualize(self, window):
        drawRect(window, self.boundary)
        if self.divided:
            self.northeast.visualize(window)
            self.northwest.visualize(window)
            self.southeast.visualize(window)
            self.southwest.visualize(window)

    def print(self):
        print(f"Boundary: {self.boundary}, Particles: {len(self.particles)}")
        if self.divided:
            self.northeast.print()
            self.northwest.print()
            self.southeast.print()
            self.southwest.print()
    

def createWindow(width, height):
    pygame.init()
    window = pygame.display.set_mode((width, height))
    pygame.display.set_caption("Quad Tree Visualization")
    return window

def drawParticle(window, particle):
    pygame.draw.circle(window, particle.color, (particle.x, particle.y), particle.radius)
    
def drawRect(window, rect, color=(0, 0, 255), thickness=1):
    pygame.draw.rect(window, color, rect, thickness)

def genRandomParticles(num_particles, width, height):
    particles = []
    for _ in range(num_particles):
        x = random.gauss(width/2, 150)
        y = random.gauss(height/2, 150)
        particle = Particle(x, y)
        particles.append(particle)
    return particles


def main():
    width, height = 800, 600
    window = createWindow(width, height)
    clock = pygame.time.Clock()
    selector = pygame.Rect(0, 0, 100, 100)
    show_selector = False

    particles = []
    quad_tree = QuadTree(pygame.Rect(0, 0, width, height), 5)

    particles = genRandomParticles(300, width, height)
    for particle in particles:
        quad_tree.insert(particle)

    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

        window.fill((0, 0, 0))
        
        input = Input()
        if input.is_mouse_button_down(0):
            mouse_x, mouse_y = input.get_mouse_position()
            particle = Particle(mouse_x, mouse_y)
            particles.append(particle)
            quad_tree.insert(particle)
        
        for particle in particles:
            particle.color = (255, 255, 255)
            particle.radius = 3
        if input.is_mouse_button_down(2):
            found = []
            mouse_x, mouse_y = input.get_mouse_position()
            selector.center = (mouse_x, mouse_y)
            show_selector = True
            quad_tree.query(selector, found)
            for p in found:
                p.color = (0, 255, 0)
                p.radius = 6
        
        if not input.is_mouse_button_down(2):
            show_selector = False

        for particle in particles:
            drawParticle(window, particle)

        quad_tree.visualize(window)
        
        if show_selector:   
            drawRect(window, selector, color=(255, 0, 0), thickness=2)

        if input.is_key_down(pygame.K_r):
            particles = []
            quad_tree = QuadTree(pygame.Rect(0, 0, width, height), 5)

            particles = genRandomParticles(300, width, height)
            for particle in particles:
                quad_tree.insert(particle)

        pygame.display.flip()
        clock.tick(60)

    pygame.quit()

if __name__ == "__main__":
    main()