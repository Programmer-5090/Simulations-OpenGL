#!/usr/bin/env python3
"""
Try to create a GL context using one of several common backends and print
compute work group limits. Tries: glfw, pyglet, GLUT (in that order).

Usage: python scripts/query_compute_limits.py

This script only depends on PyOpenGL; for context creation it will attempt
backends in order — install one of them if missing (e.g. pip install glfw pyglet).
"""
import sys
import traceback

from OpenGL import GL
from OpenGL.GL import GL_MAX_COMPUTE_WORK_GROUP_SIZE, GL_MAX_COMPUTE_WORK_GROUP_COUNT, GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS


def query_and_print():
    # Query and print limits
    try:
        import ctypes
        s0 = ctypes.c_int(); s1 = ctypes.c_int(); s2 = ctypes.c_int()
        GL.glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, ctypes.byref(s0))
        GL.glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, ctypes.byref(s1))
        GL.glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, ctypes.byref(s2))

        c0 = ctypes.c_int(); c1 = ctypes.c_int(); c2 = ctypes.c_int()
        GL.glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, ctypes.byref(c0))
        GL.glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, ctypes.byref(c1))
        GL.glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, ctypes.byref(c2))

        inv = ctypes.c_int()
        GL.glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, ctypes.byref(inv))

        print("Max Compute Work Group Size:", s0.value, "x", s1.value, "x", s2.value)
        print("Max Compute Work Group Count:", c0.value, "x", c1.value, "x", c2.value)
        print("Max Compute Work Group Invocations:", inv.value)
    except Exception:
        print("Failed to query GL limits:")
        traceback.print_exc()


# Backend: pygame (preferred if installed)
try:
    import pygame
    from pygame.locals import OPENGL, DOUBLEBUF
    print("Trying pygame...")
    pygame.init()
    # Create a tiny hidden window/context
    # Some pygame builds support HIDDEN, but creating a 1x1 window is portable.
    screen = pygame.display.set_mode((1, 1), OPENGL | DOUBLEBUF)
    try:
        query_and_print()
    finally:
        pygame.display.quit()
        pygame.quit()
    sys.exit(0)
except Exception:
    print("pygame backend not available or failed; trying next...")

# Backend: glfw
try:
    import glfw
    print("Trying GLFW...")
    if not glfw.init():
        print("glfw.init() failed")
    else:
        glfw.window_hint(glfw.VISIBLE, glfw.FALSE)
        window = glfw.create_window(1, 1, "", None, None)
        if not window:
            print("glfw.create_window failed")
            glfw.terminate()
        else:
            glfw.make_context_current(window)
            query_and_print()
            glfw.destroy_window(window)
            glfw.terminate()
            sys.exit(0)
except Exception:
    print("GLFW backend not available or failed; trying next...")

# Backend: pyglet
try:
    import pyglet
    print("Trying pyglet...")
    config = pyglet.gl.Config(double_buffer=False)
    win = pyglet.window.Window(width=1, height=1, visible=False, config=config)
    try:
        query_and_print()
    finally:
        win.close()
    sys.exit(0)
except Exception:
    print("pyglet backend not available or failed; trying next...")

# Backend: GLUT
try:
    from OpenGL.GLUT import glutInit, glutCreateWindow, glutInitDisplayMode, GLUT_DOUBLE, GLUT_RGBA
    print("Trying GLUT (freeglut)...")
    glutInit(sys.argv)
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA)
    glutCreateWindow(b"")
    query_and_print()
    sys.exit(0)
except Exception:
    print("GLUT backend not available or failed.")

print("No available context backend succeeded. Install one of: pygame, glfw, pyglet, freeglut (and its Python bindings).")
print("Examples: pip install pygame  # or pip install glfw pyglet")
sys.exit(1)
