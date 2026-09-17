document.getElementById('year').textContent = new Date().getFullYear();
const links=document.querySelectorAll('nav a');
const sections=[...document.querySelectorAll('main section[id]')];
window.addEventListener('scroll',()=>{let current='';sections.forEach(s=>{if(scrollY>=s.offsetTop-140)current=s.id});links.forEach(a=>a.classList.toggle('active',a.getAttribute('href')==='#'+current));});