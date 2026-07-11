#pragma once

import PlaygroundEngine;
import std;

namespace PgE
{
	std::unique_ptr<AppDescriptorBase> GetAppDescriptor(CommandLine commandLine);
}
