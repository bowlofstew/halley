#include "resource_filesystem.h"
#include "resources/resource_locator.h"
#include <iostream>
#include <set>
#include <halley/support/exception.h>
#include "resource_pack.h"
#include "halley/support/logger.h"
#include "api/system_api.h"

using namespace Halley;

ResourceLocator::ResourceLocator(SystemAPI& system)
	: system(system)
{
}

void ResourceLocator::add(std::unique_ptr<IResourceLocatorProvider> locator)
{
	auto& db = locator->getAssetDatabase();
	for (auto& asset: db.getAssets()) {
		auto result = locators.find(asset);
		if (result == locators.end() || result->second->getPriority() < locator->getPriority()) {
			locators[asset] = locator.get();
		}
	}
	locatorList.emplace_back(std::move(locator));
}

std::unique_ptr<ResourceData> ResourceLocator::getResource(const String& asset, AssetType type, bool stream)
{
	auto result = locators.find(asset);
	if (result != locators.end()) {
		auto data = result->second->getData(asset, type, stream);
		if (data) {
			return data;
		} else {
			throw Exception("Unable to load resource: " + asset, HalleyExceptions::Resources);
		}
	} else {
		throw Exception("Unable to locate resource: " + asset, HalleyExceptions::Resources);
	}
}

std::unique_ptr<ResourceDataStatic> ResourceLocator::getStatic(const String& asset, AssetType type)
{
	auto rawPtr = getResource(asset, type, false).release();
	auto ptr = dynamic_cast<ResourceDataStatic*>(rawPtr);
	if (!ptr) {
		delete rawPtr;
		throw Exception("Resource " + asset + " obtained, but is not static data.", HalleyExceptions::Resources);
	}
	return std::unique_ptr<ResourceDataStatic>(ptr);
}

std::unique_ptr<ResourceDataStream> ResourceLocator::getStream(const String& asset, AssetType type)
{
	auto rawPtr = getResource(asset, type, true).release();
	auto ptr = dynamic_cast<ResourceDataStream*>(rawPtr);
	if (!ptr) {
		delete rawPtr;
		throw Exception("Resource " + asset + " obtained, but is not stream data.", HalleyExceptions::Resources);
	}
	return std::unique_ptr<ResourceDataStream>(ptr);
}

void ResourceLocator::purge(const String& asset, AssetType type)
{
	auto result = locators.find(asset);
	if (result != locators.end()) {
		// Found the locator for this file, purge it
		result->second->purge(system);
	} else {
		// Couldn't find a locator (new file?), purge everything
		for (auto& l: locatorList) {
			l->purge(system);
		}
	}
}

std::vector<String> ResourceLocator::enumerate(const AssetType type)
{
	std::vector<String> result;
	for (auto& l: locatorList) {
		for (auto& r: l->getAssetDatabase().enumerate(type)) {
			result.push_back(std::move(r));
		}
	}
	return result;
}

void ResourceLocator::addFileSystem(const Path& path)
{
	add(std::make_unique<FileSystemResourceLocator>(system, path));
}

void ResourceLocator::addPack(const Path& path, const String& encryptionKey, bool preLoad, bool allowFailure)
{
	auto dataReader = system.getDataReader(path.string());
	if (dataReader) {
		add(std::make_unique<PackResourceLocator>(std::move(dataReader), path, encryptionKey, preLoad));
	} else {
		if (allowFailure) {
			Logger::logWarning("Resource pack not found: \"" + path.string() + "\"");
		} else {
			throw Exception("Unable to load resource pack \"" + path.string() + "\"", HalleyExceptions::Resources);
		}
	}
}

const Metadata& ResourceLocator::getMetaData(const String& asset, AssetType type) const
{
	auto result = locators.find(asset);
	if (result != locators.end()) {
		return result->second->getAssetDatabase().getDatabase(type).get(asset).meta;
	} else {
		throw Exception("Unable to locate resource: " + asset, HalleyExceptions::Resources);
	}
}

bool ResourceLocator::exists(const String& asset)
{
	return locators.find(asset) != locators.end();
}
