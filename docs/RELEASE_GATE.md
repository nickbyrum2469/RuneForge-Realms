# Release Gate

A RuneForge release may reach `main` only after the integration branch passes the portable core test lane and the native Windows/Vulkan/shader build lane. The merged commit must then rebuild successfully before the Windows release package is considered publishable.
