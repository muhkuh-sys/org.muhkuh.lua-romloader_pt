import os
import platform
import re
import string
import subprocess


class PlatformDetect:
    def __init__(self):
        self.strHostCpuArchitecture = None
        self.strHostDistributionId = None
        self.strHostDistributionVersion = None
        self.strStandardArchiveFormat = None

    def __windows_get_cpu_architecture_env(self):
        strCpuArchitecture = None
        strEnvProcessorArchitecture = None
        strEnvProcessorArchiteW6432 = None
        if 'PROCESSOR_ARCHITECTURE' in os.environ:
            strEnvProcessorArchitecture = os.environ[
                'PROCESSOR_ARCHITECTURE'
            ].lower()
        if 'PROCESSOR_ARCHITEW6432' in os.environ:
            strEnvProcessorArchiteW6432 = os.environ[
                'PROCESSOR_ARCHITEW6432'
            ].lower()
        # See here for details: https://blogs.msdn.microsoft.com/david.wang/
        # 2006/03/27/howto-detect-process-bitness/
        if((strEnvProcessorArchitecture == 'amd64') or
           (strEnvProcessorArchiteW6432 == 'amd64')):
            strCpuArchitecture = 'x86_64'
        elif((strEnvProcessorArchitecture == 'x86') and
             (strEnvProcessorArchiteW6432 is None)):
            strCpuArchitecture = 'x86'
        else:
            print('Failed to detect the CPU architecture on Windows with the '
                  'ENV variables.')
            print('PROCESSOR_ARCHITECTURE = %s' %
                  (str(strEnvProcessorArchitecture)))
            print('PROCESSOR_ARCHITEW6432 = %s' %
                  (str(strEnvProcessorArchiteW6432)))

        return strCpuArchitecture

    def __linux_get_dpkg_architecture(self):
        strCpuArchitecture = None
        astrReplacements = {
            'amd64': 'x86_64'
        }

        # Try to get the architecture from the 'dpkg' command.
        strCpuArchitecture = subprocess.check_output(['dpkg', '--print-architecture']).decode(
            "utf-8",
            "replace"
        ).strip()

        # Replace the CPU architectures found in the list.
        if strCpuArchitecture in astrReplacements:
            strCpuArchitecture = astrReplacements[strCpuArchitecture]

        return strCpuArchitecture

    def __linux_detect_distribution_etc_lsb_release(self):
        strDistributionId = None
        strDistributionVersion = None

        # Try to open /etc/lsb-release.
        tFile = open('/etc/lsb-release', 'rt')
        if tFile is None:
            raise Exception('Failed to detect the Linux distribution with '
                            '/etc/lsb-release.')
        for strLine in tFile:
            tMatch = re.match(r'DISTRIB_ID=(.+)', strLine)
            if tMatch is not None:
                strDistributionId = tMatch.group(1).lower()
            tMatch = re.match(r'DISTRIB_RELEASE=(.+)', strLine)
            if tMatch is not None:
                strDistributionVersion = tMatch.group(1)
        tFile.close()

        # Return both components or none.
        if (strDistributionId is None) or (strDistributionVersion is None):
            strDistributionId = None
            strDistributionVersion = None

        return strDistributionId, strDistributionVersion

    def detect(self):
        strSystem = platform.system()
        if strSystem == 'Windows':
            # This is windows.

            # Detect the CPU architecture.
            self.strHostCpuArchitecture =\
                self.__windows_get_cpu_architecture_env()

            # Set the distribution version and ID.
            self.strHostDistributionId = 'windows'
            self.strHostDistributionVersion = None

            # Windows uses ZIP as standard archive format.
            self.strStandardArchiveFormat = 'zip'
        elif strSystem == 'Linux':
            # This is a Linux.

            # Detect the CPU architecture.
            strCpuArch = self.__linux_get_dpkg_architecture()
            self.strHostCpuArchitecture = strCpuArch

            # Detect the distribution.
            self.strHostDistributionId, self.strHostDistributionVersion =\
                self.__linux_detect_distribution_etc_lsb_release()

            # Linux uses TAR GZIP as standard archive format.
            self.strStandardArchiveFormat = 'tar.gz'
        else:
            raise Exception('Unknown platform: "%s"' % (strSystem))
