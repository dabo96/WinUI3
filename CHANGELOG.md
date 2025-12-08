# Changelog

Todos los cambios notables en este proyecto serán documentados en este archivo.

El formato está basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.0.0/),
y este proyecto adhiere a [Semantic Versioning](https://semver.org/lang/es/).

## [1.0.0] - 2024

### ✨ Añadido
- Sistema completo de widgets básicos y avanzados
- Renderizado MSDF de alta calidad para texto
- Generación dinámica de MSDF desde FreeType
- Sistema de temas Fluent (Light/Dark)
- Colores de acento configurables
- Sistema de layout vertical/horizontal con constraints
- Efectos visuales Fluent (elevation, acrylic, ripple)
- Animaciones suaves para transiciones de estado
- Batch rendering para optimización de rendimiento
- Sistema completo de input (mouse y teclado)
- ScrollView con scrollbars automáticos
- TabView con scroll independiente por tab
- ListView y TreeView
- Modal/Dialog arrastrable
- MenuBar y ContextMenu
- Tooltips informativos
- Sistema de contraste de fondo inteligente (sin bordes, estilo Windows Settings)

### 🔧 Cambiado
- Mejorado renderizado de texto MSDF para mayor nitidez
- Optimizado sistema de scroll para prevenir conflictos entre widgets
- Mejorado manejo de espacios en texto MSDF
- Refinado sistema de contraste visual sin bordes

### 🐛 Corregido
- Scroll compartido entre tabs en TabView
- Procesamiento múltiple de eventos de scroll
- Posicionamiento incorrecto de items en menús dropdown
- Transparencias y nitidez en renderizado MSDF
- Manejo de espacios en texto renderizado

### 📚 Documentación
- API_REFERENCE.md completo (419 líneas)
- README.md actualizado con estado real del proyecto
- PLAN_DE_TRABAJO.md con fases completadas
- Ejemplo funcional completo (examples/main.cpp)

---

## [Unreleased]

### Planificado para v1.1+
- Optimizaciones avanzadas de rendimiento (culling, pooling)
- Tests automatizados
- Ejemplos individuales por widget
- Documentación adicional (guías avanzadas)
- Soporte para múltiples ventanas
- Sistema de drag & drop
- Soporte para imágenes/texturas en widgets

