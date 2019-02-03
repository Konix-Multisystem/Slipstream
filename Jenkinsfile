#!groovy

pipeline {
    agent none
    stages {
        stage('Build And Test') {
            parallel {
		stage('build linux')
		{
	            agent { label "linux" }
		    steps
		    {
			notifyBuild('STARTED')

			checkout scm

			sh '''rm -rf buildlinux
				mkdir buildlinux
				cd buildlinux
				cmake -DCMAKE_BUILD_TYPE="Release" -DEDL_COMMAND=/usr/local/bin/edl ..
				make all'''
		    }
	            post {
	                failure
                        {
			    notifyBuild("FAILED")
		        }
		        success
		        {
			    archiveArtifacts artifacts: 'buildlinux/slipstream'
			    notifyBuild("SUCCESS")
			}
		    }
		}
		stage('build mojave')
		{
	            agent { label "macos" }
		    steps
		    {
			notifyBuild('STARTED')

			checkout scm

			sh '''rm -rf buildmacos
				mkdir buildmacos
				cd buildmacos
				cmake -DCMAKE_BUILD_TYPE="Release" -DEDL_COMMAND=/Users/jenkins/bin/edl ..
				make all'''
		    }
	            post {
	                failure
                        {
			    notifyBuild("FAILED")
		        }
		        success
		        {
			    archiveArtifacts artifacts: 'buildmacos/slipstream'
			    notifyBuild("SUCCESS")
			}
		    }
		}
		stage('build windows 64')
		{
	            agent { label "windows" }
		    steps
		    {
			notifyBuild('STARTED')

			checkout scm

			bat ''' rmdir /S/Q build64
				mkdir build64
				cd build64
				cmake -Thost=x64 -G "Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE="Release" -Dglfw3_DIR=%TOOLS_ROOT64%\\glfw-ins\\lib\\cmake\\glfw3 -DOPENAL_LIBRARY=%TOOLS_ROOT64%\\openal-ins\\lib\\openal32.lib -DOPENAL_INCLUDE_DIR=%TOOLS_ROOT64%\\openal-ins\\include -DEDL_COMMAND=%TOOLS_ROOT64%\\edl.exe ..
				cmake --build . --target ALL_BUILD --config Release'''
		    }
	            post {
	                failure
                        {
			    notifyBuild("FAILED")
		        }
		        success
		        {
			    archiveArtifacts artifacts: 'build64/Release/*.exe'
			    notifyBuild("SUCCESS")
			}
		    }
		}
/*		stage('build windows 32')
		{
	            agent { label "windows" }
		    steps
		    {
			notifyBuild('STARTED')

			checkout scm

			bat ''' rmdir /S/Q build32
				mkdir build32
				cd build32
				cmake -G "Visual Studio 15 2017" -DCMAKE_BUILD_TYPE="Release" -Dglfw3_DIR=%TOOLS_ROOT32%\\glfw-ins\\lib\\cmake\\glfw3 -DOPENAL_LIBRARY=%TOOLS_ROOT32%\\openal-ins\\lib\\openal32.lib -DOPENAL_INCLUDE_DIR=%TOOLS_ROOT32%\\openal-ins\\include -DEDL_COMMAND=%TOOLS_ROOT32%\\edl.exe ..
				cmake --build . --target ALL_BUILD --config Release'''
		    }
	            post {
	                failure
                        {
			    notifyBuild("FAILED")
		        }
		        success
		        {
			    archiveArtifacts artifacts: 'build32/Release/*.exe'
			    notifyBuild("SUCCESS")
			}
		    }
		}*/
	    }
	}
    }
}

def notifyBuild(String buildStatus = 'STARTED') {
  // build status of null means successful
  buildStatus =  buildStatus ?: 'SUCCESSFUL'
 
  // Default values
  def colorName = 'RED'
  def colorCode = '#FF0000'
  def subject = "${buildStatus}: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]'"
  def summary = "${subject} (${env.BUILD_URL})"
  def details = '${SCRIPT, template="groovy-text.template"}'
 
  // Override default values based on build status
  if (buildStatus == 'STARTED') {
    color = 'YELLOW'
    colorCode = '#FFFF00'
  } else if (buildStatus == 'SUCCESSFUL') {
    color = 'GREEN'
    colorCode = '#00FF00'
  } else {
    color = 'RED'
    colorCode = '#FF0000'
  }
 
  // Send notifications
 
  emailext (
      subject: subject,
      body: details,
      recipientProviders: [[$class: 'DevelopersRecipientProvider'],[$class: 'RequesterRecipientProvider']]
    )
}
