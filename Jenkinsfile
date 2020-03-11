pipeline {
    agent { dockerfile true }

    stages {
        stage('Build') {
            steps {
                echo 'Building..'
                sh 'bash ./build.sh'
                echo 'Finished building.'
            }
        }
        stage('Test') {
            steps {
                echo 'Testing..'
                sh 'bash ./test.sh'
                echo 'Finished testing.'
            }
        }
    }
    post{
        always{
            sh 'apt list --installed > current_packages.txt'
            archiveArtifacts artifacts: 'current_packages.txt', fingerprint: true

            sh './hat_env/bin/pip3 freeze > requirements.txt'
            archiveArtifacts artifacts: 'requirements.txt', fingerprint: true

            echo 'Archive your test results here'
            //archiveArtifacts artifacts: 'build/tests/rsa/release/bin/rsa.elf', fingerprint: true
            //archiveArtifacts artifacts: 'build/tests/sha256/release/bin/sha256.elf', fingerprint: true
        }
        success{
            echo 'Archive your build results here'
            //archiveArtifacts artifacts: 'build/apps/firmware/release/bin/firmware-1.elf', fingerprint: true
            //archiveArtifacts artifacts: 'build/apps/firmware/release/bin/firmware-0.elf', fingerprint: true
            //archiveArtifacts artifacts: 'build/apps/bootloader/release/bin/bootloader.elf', fingerprint: true
        }
        fixed{
            script {
                if (env.BRANCH_NAME in ['master']) {
                    slackSend (color: '#00FF00', channel: "#jenkins",  message: "FIXED: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]' (${env.RUN_DISPLAY_URL})")
                }
            }
        }
        regression{
            script {
                if (env.BRANCH_NAME in ['master']) {
                    slackSend (color: '#FF0000', channel: "#jenkins", message: "REGRESSED: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]' (${env.RUN_DISPLAY_URL})")
                    emailext (
                      subject: "REGRESSED: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]'",
                      body: """REGRESSED: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]':
                      Check console output at ${env.BUILD_URL}""",
                      recipientProviders: [[$class: 'CulpritsRecipientProvider']]
                      )
                }
            }
        }
    }
}