let myImg = document.querySelector('img');

myImg.onclick = function() {
    let mySrc = myImg.getAttribute('src');
    if (mySrc === 'images/test1.jpg') {
        myImg.setAttribute('src', 'images/test3.jpg');
    } else {
        myImg.setAttribute('src', 'images/test1.jpg');
    }
}

/*
let myButton = document.querySelector('button');
let myHeading = document.querySelector('h1');

function setUserName() {
    let myName = prompt('请输入你的名字');
    localStorage.setItem('name', myName);
    myHeading.textContent = '欢迎来到Modern C++的世界, ' + myName;
}

if (!localStorage.getItem('name')) {
    setUserName();
} else {
    let storedName = localStorage.getItem('name');
    myHeading.textContent = '欢迎来到Modern C++的世界, ' + storedName;
}

myButton.onclick = function() {
    setUserName()
}
*/
