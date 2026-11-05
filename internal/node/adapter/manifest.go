package adapter

import "sort"

// Manifest describes an Adapter implementation for discovery, capability
// negotiation and the public Node information endpoint.
type Manifest struct {
	ID           string
	Name         string
	Version      string
	APIVersion   int
	Runtime      string
	Transport    []string
	Capabilities []string
	Permissions  []string
}

// ManifestProvider is implemented by adapters that want to describe themselves
// explicitly. Adapters without it get a conservative default manifest.
type ManifestProvider interface {
	Manifest() Manifest
}

// ManifestFor returns the adapter's declared manifest or a conservative
// default derived from its type and capabilities.
func ManifestFor(a AgentAdapter) Manifest {
	if a == nil {
		return Manifest{}
	}
	if provider, ok := a.(ManifestProvider); ok {
		manifest := provider.Manifest()
		if manifest.ID == "" {
			manifest.ID = a.Type()
		}
		if manifest.Name == "" {
			manifest.Name = manifest.ID
		}
		if manifest.APIVersion == 0 {
			manifest.APIVersion = 1
		}
		return manifest
	}
	manifest := Manifest{
		ID:         a.Type(),
		Name:       a.Type(),
		Version:    "0.0.0",
		APIVersion: 1,
		Runtime:    "local",
	}
	for name, enabled := range a.Capabilities() {
		if enabled {
			manifest.Capabilities = append(manifest.Capabilities, name)
		}
	}
	sort.Strings(manifest.Capabilities)
	return manifest
}
