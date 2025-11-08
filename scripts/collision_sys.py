from quad_tree import *
import numpy as np
import pygame
import random

WIDTH, HEIGHT = 800, 600


class CollisionSystem:
    def __init__(self, width=WIDTH, height=HEIGHT):
        self.quad_tree = None
        self.particles = []
        self.gravity = 200.0 * 10
        self.iterations = 5
        self.width = width
        self.height = height

    def update(self, dt):
        self.set_up_quad_tree()
        
        sub_dt = dt / self.iterations
        for _ in range(self.iterations):
            self.apply_gravity(sub_dt)
            self.check_collisions()
            self.update_particles(sub_dt)
            self.wall_collisions()

    def set_up_quad_tree(self):
        self.quad_tree = QuadTree(pygame.Rect(0, 0, self.width, self.height), 5)
        for particle in self.particles:
            self.quad_tree.insert(particle)

    def create_particle(self, pos, dt=1):
        particle = Particle(pos[0], pos[1])
        particle.color = (random.randint(100, 255),
                          random.randint(100, 255),
                          random.randint(100, 255))
        particle.radius = 6
        particle.accel = (0.0, 0.0)
        init_vx = random.uniform(-0.5, 0.5)
        init_vy = random.uniform(-0.5, 0.5)

        particle.setVelocity(init_vx, init_vy, dt)

        return particle

    def add_particle(self, particle):
        self.particles.append(particle)
        if self.quad_tree:
            self.quad_tree.insert(particle)
    
    def wall_collisions(self):
        restitution = 0.8
        for particle in self.particles:
            # Left/Right walls
            if particle.x - particle.radius < 0:
                particle.x = particle.radius
                vx, vy = particle.getVelocity()
                particle.setVelocity(-vx * restitution, vy, dt=1.0)
            elif particle.x + particle.radius > self.width:
                particle.x = self.width - particle.radius
                vx, vy = particle.getVelocity()
                particle.setVelocity(-vx * restitution, vy, dt=1.0)
            
            # Bottom/Top walls  
            if particle.y - particle.radius < 0:
                particle.y = particle.radius
                vx, vy = particle.getVelocity()
                particle.setVelocity(vx, -vy * restitution, dt=1.0)
            elif particle.y + particle.radius > self.height:
                particle.y = self.height - particle.radius
                vx, vy = particle.getVelocity()
                particle.setVelocity(vx, -vy * restitution, dt=1.0)

    def resolveCollision(self, p1, p2):
        resp_coefficient = 1.0
        eps = 1e-6
        
        dx = p2.x - p1.x
        dy = p2.y - p1.y
        dist2 = dx * dx + dy * dy
        
        min_dist = p1.radius + p2.radius
        min_dist2 = min_dist * min_dist
        
        if dist2 < min_dist2 and dist2 > eps:
            dist = np.sqrt(dist2)
            
            overlap = min_dist - dist
            delta = resp_coefficient * 0.5 * overlap  # Each particle moves half
            
            # Normalized direction vector
            nx = dx / dist
            ny = dy / dist
            
            p1.x -= nx * delta
            p1.y -= ny * delta
            p2.x += nx * delta
            p2.y += ny * delta

    def check_collisions(self):
        if not self.quad_tree:
            return
        for particle in self.particles:
            found = []
            r = pygame.Rect(int(particle.x - particle.radius), int(particle.y - particle.radius),
                            int(particle.radius * 2), int(particle.radius * 2))
            self.quad_tree.query(r, found)
            for other in found:
                if other != particle:
                    self.resolveCollision(particle, other)

    def update_particles(self, dt=1.0):
        max_vel = 500.0
        max_vel_sq = max_vel * max_vel
        
        for particle in self.particles:
            particle.update(dt)

    def apply_gravity(self, dt):
        for particle in self.particles:
            particle.accelerate(0, self.gravity * dt)
            
            
def main():
    pygame.init()
    window = createWindow(WIDTH, HEIGHT)
    clock = pygame.time.Clock()

    cs = CollisionSystem(WIDTH, HEIGHT)
    cs.set_up_quad_tree()

    spawn_acc = 0.0
    spawn_rate = 0.01

    running = True
    while running:
        dt = clock.tick(60) / 1000.0  # seconds

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

        mouse_buttons = pygame.mouse.get_pressed()
        mouse_pos = pygame.mouse.get_pos()
        if mouse_buttons[0]:
            spawn_acc += dt
            while spawn_acc >= spawn_rate:
                p = cs.create_particle(mouse_pos, dt=1.0)
                cs.add_particle(p)
                spawn_acc -= spawn_rate
        else:
            spawn_acc = 0.0

        cs.update(dt)

        # render
        window.fill((0, 0, 0))
        if cs.quad_tree:
            cs.quad_tree.visualize(window)

        for particle in cs.particles:
            drawParticle(window, particle)

        pygame.display.flip()

    pygame.quit()


if __name__ == "__main__":
    main()


